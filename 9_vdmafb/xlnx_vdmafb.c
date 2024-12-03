#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/dma/xilinx_dma.h>
#include <linux/of_dma.h>
#include <video/videomode.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <video/of_videomode.h>
#include "linux/device.h"
#include "linux/gpio.h"
#include "xilinx-vtc.h"



/*LCD屏硬件ID*/
#define ATK1018 5      //10寸 1280*800


/*自定义结构体用于描述我们的LCD设备*/
struct xilinx_vdmafb_dev
{
    struct fb_info *fb_info;        /*framebuffer设备信息结构体*/
    struct platform_device *pdev;   /*platform_device结构体*/
    struct clk *pclk;               /*像素时钟*/
    struct xvtc_device *vtc;         /*vtc设备*/
    struct dma_chan *vdma;          /*VDMA通道*/
};


int custom_fb_videomode_from_videomode(const struct videomode *vm,
				struct fb_videomode *fbmode)
{
	unsigned int htotal, vtotal, total;

	fbmode->xres = vm->hactive;
	fbmode->left_margin = vm->hback_porch;
	fbmode->right_margin = vm->hfront_porch;
	fbmode->hsync_len = vm->hsync_len;

	fbmode->yres = vm->vactive;
	fbmode->upper_margin = vm->vback_porch;
	fbmode->lower_margin = vm->vfront_porch;
	fbmode->vsync_len = vm->vsync_len;

	/* prevent division by zero in KHZ2PICOS macro */
	fbmode->pixclock = vm->pixelclock ?
			KHZ2PICOS(vm->pixelclock / 1000) : 0;

	fbmode->sync = 0;
	fbmode->vmode = 0;
	if (vm->flags & DISPLAY_FLAGS_HSYNC_HIGH)
		fbmode->sync |= FB_SYNC_HOR_HIGH_ACT;
	if (vm->flags & DISPLAY_FLAGS_VSYNC_HIGH)
		fbmode->sync |= FB_SYNC_VERT_HIGH_ACT;
	if (vm->flags & DISPLAY_FLAGS_INTERLACED)
		fbmode->vmode |= FB_VMODE_INTERLACED;
	if (vm->flags & DISPLAY_FLAGS_DOUBLESCAN)
		fbmode->vmode |= FB_VMODE_DOUBLE;
	fbmode->flag = 0;

	htotal = vm->hactive + vm->hfront_porch + vm->hback_porch +
		 vm->hsync_len;
	vtotal = vm->vactive + vm->vfront_porch + vm->vback_porch +
		 vm->vsync_len;
	/* prevent division by zero */
	total = htotal * vtotal;
	if (total) {
		fbmode->refresh = vm->pixelclock / total;
	/* a mode must have htotal and vtotal != 0 or it is invalid */
	} else {
		fbmode->refresh = 0;
		return -EINVAL;
	}

	return 0;
}



/*伪调色板*/
static int vdmafb_setcolreg(unsigned regno,unsigned red,unsigned green,unsigned blue,unsigned transp,struct fb_info *info)
{
    u32 tmp;

    if(regno >=16) //最多支持16个索引
        return 1;

    red >>= 8;green >>= 8;blue >>= 8;
    tmp = (red << 16) | (green << 8) | blue;
    ((u32 *)info->pseudo_palette)[regno] = tmp;


    return 0;
}

/*检查显示模式*/
static int vdmafb_check_var(struct fb_var_screeninfo *var,struct fb_info *info)
{
    struct fb_var_screeninfo *fb_var = &info->var;
    memcpy(var,fb_var,sizeof(struct fb_var_screeninfo));
    return 0;
}

/*Frame Buffer操作函数集*/
static struct fb_ops xilinx_vdmafb_ops = {
    .owner = THIS_MODULE,
    .fb_setcolreg = vdmafb_setcolreg,
    .fb_check_var = vdmafb_check_var,
    .fb_fillrect = cfb_fillrect,
    .fb_copyarea = cfb_copyarea,
    .fb_imageblit = cfb_imageblit,
};



static int vdmafb_init_fbinfo_dt(struct xilinx_vdmafb_dev *fbdev,struct videomode *vmode)
{   
    struct device *dev = &fbdev->pdev->dev;
    int display_timing;
    //struct gpio_desc *lcd_gpios[3];
    int lcd_gpios[3];
    int lcd_id = 0;
    int i;
    int ret;

    // /* 获取 LCD ID GPIOs */
    // for (i = 0; i < 2; i++) {
    //     lcd_gpios[i] = devm_gpiod_get_index(dev, "lcdID", i, GPIOF_IN);
    //     if (IS_ERR(lcd_gpios[i])) {
    //         dev_err(dev, "Failed to get LCD ID GPIO %d\n", i);
    //         lcd_id = ATK1018;
    //         return PTR_ERR(lcd_gpios[i]);
    //     }
    //     gpiod_direction_input(lcd_gpios[i]); // 确保方向为输入
    //     msleep(5);  //延时5ms
    //     //输出到终端
    //     dev_info(dev, "LCD ID GPIO %d: %d\n", i, gpiod_get_value(lcd_gpios[i]));
    //     lcd_id |= gpiod_get_value(lcd_gpios[i]) << i;   //读取LCD ID
    // }

    for(i=0;i<3;i++)
    {
        lcd_gpios[i] = of_get_named_gpio(dev->of_node, "lcdID-gpio", i);
        if(lcd_gpios[i] < 0)
        {
            dev_err(dev, "Failed to get LCD ID GPIO %d\n", i);
            lcd_id = ATK1018;
            return -ENODEV;
        }
        ret = devm_gpio_request_one(dev,lcd_gpios[i],GPIOF_IN,"LCD ID");
        if (ret) {
            dev_err(dev, "Failed to request lcdID-gpio %d\n", i);
            return ret;
        }
        /* 3. 读取 GPIO 值 */
        gpio_direction_input(lcd_gpios[i]);
        lcd_id |= gpio_get_value(lcd_gpios[i]) << i;   //读取LCD ID
        
    }

    dev_info(dev, "LCD ID: %d\n", lcd_id);  //打印LCD ID

    /*再将ID引脚设置为输出模式*/
    msleep(5);
    for (i = 0; i < 3; i++) {
        gpio_direction_output(lcd_gpios[i], 0);
    }

    /*获取LCD显示时序参数*/
    switch (lcd_id) {
    case ATK1018:display_timing = 0;break;
    default:
        display_timing = 0;
        dev_info(dev, "LCD ID not supported, using default timing\n");
        break;
    }

    ret = of_get_videomode(dev->of_node, vmode, display_timing);
    if (ret) {
        dev_err(dev, "Failed to get videomode\n");
        return ret;
    }

    return 0;

}



static int vdmafb_init_fbinfo(struct xilinx_vdmafb_dev *fbdev,struct videomode *vmode)
{
    struct device *dev = &fbdev->pdev->dev;
    struct fb_info *info = fbdev->fb_info;
    struct fb_videomode mode = {0};
    dma_addr_t fb_phys;         //显存物理地址
    void *fb_virt;              //显存虚拟地址
    unsigned fb_size;           //显存大小
    int ret;

    /*解析设备树获取LCD时序参数*/
    ret = vdmafb_init_fbinfo_dt(fbdev, vmode);
    if (ret<0) {
        dev_err(dev, "Failed to get videomode\n");
        return ret;
    }

    /*申请LCD显存*/
    fb_size = vmode->hactive * vmode->vactive * 3;
    fb_virt = dma_alloc_wc(dev, PAGE_ALIGN(fb_size), &fb_phys, GFP_KERNEL);
    if (!fb_virt) {
        dev_err(dev, "Failed to allocate framebuffer\n");
        return -ENOMEM;
    }

    //打印显存物理地址和大小
    dev_info(dev, "Frame Buffer physical address: 0x%lx - 0x%lx\n",
         fb_phys, fb_phys + fb_size - 1);


    memset(fb_virt, 0, fb_size);

    /*初始化fb_info结构体*/
    info->fbops = &xilinx_vdmafb_ops;   //设置操作函数集
    info->screen_base = fb_virt;        //显存虚拟地址
    info->screen_size = fb_size;        //显存大小

    //固定属性初始化（fix）
    strcpy(info->fix.id, "xilinx-vdmafb");   //设置设备ID
    info->fix.type = FB_TYPE_PACKED_PIXELS;  //设置像素类型,像素紧密存储
    info->fix.visual = FB_VISUAL_TRUECOLOR;  //真彩色
    info->fix.accel = FB_ACCEL_NONE;         //不支持加速
    info->fix.line_length = vmode->hactive * 3;  //一行的字节数
    info->fix.smem_start = fb_phys;             //显存物理地址
    info->fix.smem_len = fb_size;                //显存大小

    //可变属性初始化（var）
    info->var.grayscale = 0;                    //彩色
    info->var.nonstd = 0;                        //标准模式
    info->var.bits_per_pixel = 24;               //24位色
    info->var.activate = FB_ACTIVATE_NOW;        //立即激活
    info->var.accel_flags = FB_ACCEL_NONE;       //不支持加速
    info->var.xres = info->var.xres_virtual = vmode->hactive;  //实际水平分辨率=虚拟水平分辨率
    info->var.yres = info->var.yres_virtual = vmode->vactive;  //实际垂直分辨率=虚拟垂直分辨率
    info->var.xoffset = info->var.yoffset = 0;                  //偏移量为0
    info->var.red.offset = 0;                                   //红色偏移量
    info->var.red.length = 8;                                   //红色位数
    info->var.green.offset = 8;                                 //绿色偏移量
    info->var.green.length = 8;                                 //绿色位数
    info->var.blue.offset = 16;                                 //蓝色偏移量
    info->var.blue.length = 8;                                  //蓝色位数
    info->var.transp.offset = 0;                                //透明度偏移量
    info->var.transp.length = 0;                                //透明度位数

    //提取设备树中的显示模式信息填充到可变属性中
    custom_fb_videomode_from_videomode(vmode, &mode);
    // 将通用的 videomode 转换为 fb_videomode 格式
    // vmode: 指向设备树或其他配置中解析出的通用 videomode 数据（如分辨率和时序参数）
    // mode: 指向 fb_videomode 结构，用于 Frame Buffer 驱动中较轻量化的模式表示             
    fb_videomode_to_var(&info->var,&mode);
    // 将 fb_videomode 转换为 fb_var_screeninfo 格式
    // info->var: Frame Buffer 可变参数结构，用于硬件寄存器配置（如分辨率、时序）
    // mode: 从上一步得到的 fb_videomode 数据，提供显示模式信息                 

    return 0;

}

static int vdmafb_init_vdma(struct xilinx_vdmafb_dev *fbdev)
{
    struct device *dev = &fbdev->pdev->dev;
    struct fb_info *info = fbdev->fb_info;
    struct dma_interleaved_template *dma_template;
    struct dma_async_tx_descriptor *tx_desc;
    struct xilinx_vdma_config vdma_config={0};
    int ret;

size_t num_sgl = 1; // 假设需要 1 个 sgl
dma_template = kzalloc(sizeof(*dma_template) + sizeof(struct data_chunk) * num_sgl, GFP_KERNEL);
if (!dma_template) {
    dev_err(dev, "Failed to allocate memory for dma_template\n");
    return -ENOMEM;
}

    dev_info(dev, "Step 1: Requesting VDMA channel\n");
    /*申请vdma通道*/
    fbdev->vdma = of_dma_request_slave_channel(dev->of_node, "lcd_vdma");
    if (IS_ERR(fbdev->vdma)) {
        dev_err(dev, "Failed to request vdma channel\n");
        return PTR_ERR(fbdev->vdma);
    }
    dev_info(dev, "Step 2: VDMA channel requested successfully\n");
    dev_info(dev, "VDMA channel address: %p\n", fbdev->vdma);
    if(!fbdev->vdma)
    {
        dev_err(dev, "Failed to get VDMA channel\n");
        return -ENODEV;
    }
    //dma_template->sgl[0].size
    dev_info(dev, "dma_template->sgl[0].size: %d\n", dma_template->sgl[0].size);


/* 终止VDMA通道数据传输 */
dev_info(dev, "Step 3: Terminating all VDMA transactions\n");
dmaengine_terminate_all(fbdev->vdma);

/* 初始化VDMA通道 */
dev_info(dev, "Step 4: Initializing VDMA template\n");
dma_template->dir = DMA_MEM_TO_DEV;  // 从内存到外设
dma_template->numf = info->var.yres; // 行数
dma_template->sgl[0].size = info->fix.line_length;  // 一行的字节数
dma_template->frame_size = 1;        // 帧大小
dma_template->sgl[0].icg = 0;        // 间隔
dma_template->src_start = info->fix.smem_start;  // 物理地址
dma_template->src_sgl = 1;           // 单个源地址分散模式
dma_template->src_inc = 1;           // 源地址递增
dma_template->dst_inc = 0;           // 目的地址固定
dma_template->dst_sgl = 0;           // 单个目的地址

/* 检查模板初始化后的值 */
dev_info(dev, "dma_template->dir: %d\n", dma_template->dir);
dev_info(dev, "dma_template->numf: %d\n", dma_template->numf);
dev_info(dev, "dma_template->sgl[0].size: %d\n", dma_template->sgl[0].size);
dev_info(dev, "dma_template->src_start: 0x%llx\n", dma_template->src_start);

/* 生成描述符 */
dev_info(dev, "Step 5: Preparing DMA descriptor\n");
tx_desc = dmaengine_prep_interleaved_dma(fbdev->vdma, dma_template, DMA_CTRL_ACK | DMA_PREP_INTERRUPT);
if (!tx_desc) {
    dev_err(dev, "Failed to prepare slave transfer\n");
    dma_release_channel(fbdev->vdma);
    return -ENOMEM;
}
dev_info(dev, "Step 6: DMA descriptor prepared successfully\n");

/* 配置VDMA通道 */
dev_info(dev, "Step 7: Configuring VDMA channel\n");
vdma_config.park = 1;
ret = xilinx_vdma_channel_set_config(fbdev->vdma, &vdma_config);
if(ret !=0)
{
    dev_info(dev,"xilinx_vdma_channel_set_config error!");
    return -ENOMEM;
}

/* 启动VDMA通道 */
dev_info(dev, "Step 8: Submitting DMA descriptor\n");
ret = dmaengine_submit(tx_desc);
if(ret < 0)
{
    dev_info(dev,"dmaengine_submit error!");
    return -ENOMEM;
}

dev_info(dev, "Step 9: Starting DMA transaction\n");
 dma_async_issue_pending(fbdev->vdma);

dev_info(dev, "Step 10: VDMA initialized successfully\n");
kfree(dma_template);


return 0;

}

static int vdmafb_init_vtc(struct xilinx_vdmafb_dev *fbdev, struct videomode *vmode)
{
    struct device_node *node;
    struct device *dev = &fbdev->pdev->dev;
    struct xvtc_config config;
    int ret;

    // /*获取VTC设备节点*/
    // node = of_parse_phandle(dev->of_node, "vtc", 0);
    // if (!node) {
    //     dev_err(dev, "Failed to get VTC node\n");
    //     return -ENODEV;
    // }


    /* 获取 VTC 设备 */
    fbdev->vtc = xvtc_of_get(dev->of_node);
    dev_info(dev, "VTC node: %s\n", dev->of_node->name);
    dev_info(dev, "VTC device address: %p\n", fbdev->vtc);

    //of_node_put(node);  //释放设备节点引用
    if(!fbdev->vtc||IS_ERR(fbdev->vtc))
    {
        dev_err(dev, "Failed to get VTC device\n");
        return -ENODEV;
    }


    /* 配置 VTC 时序参数 */
    config.hblank_start = vmode->hactive;
    config.hsync_start = vmode->hactive + vmode->hfront_porch;
    config.hsync_end = config.hsync_start + vmode->hsync_len;
    config.hsize = vmode->hactive + vmode->hfront_porch + vmode->hsync_len + vmode->hback_porch;

    config.vblank_start = vmode->vactive;
    config.vsync_start = vmode->vactive + vmode->vfront_porch;
    config.vsync_end = config.vsync_start + vmode->vsync_len;
    config.vsize = vmode->vactive + vmode->vfront_porch + vmode->vsync_len + vmode->vback_porch;
    
    config.fps = 60;
        /* 启动 VTC 生成器 */
    ret = xvtc_generator_start(fbdev->vtc, &config);
    if (ret) {
        dev_err(dev, "Failed to start VTC generator\n");
        xvtc_put(fbdev->vtc);
        return ret;
    }

    dev_info(dev, "VTC configured successfully\n");
    return 0;

}



static int vdmafb_probe(struct platform_device *pdev)
{
    struct xilinx_vdmafb_dev *fbdev;
    struct fb_info *info;
    struct videomode vmode;
    int ret;


    printk("vdmafb_probe\n");

    /*实例化一个fb_info结构体对象*/
    info = framebuffer_alloc(sizeof(struct xilinx_vdmafb_dev), &pdev->dev);
    if(!info)
    {
        dev_err(&pdev->dev, "framebuffer_alloc failed\n");
        return -ENOMEM;
    }
    
    fbdev = info->par;
    fbdev->fb_info = info;
    fbdev->pdev = pdev;

    dev_info(&pdev->dev, "Device tree node: %pOF\n", pdev->dev.of_node);
    /*获取LCD所需时钟*/
    fbdev->pclk =  devm_clk_get(&pdev->dev, "lcd_pclk");
    if(IS_ERR(fbdev->pclk))
    {
        dev_err(&pdev->dev, "failed to get lcd_pclk\n");
        ret = PTR_ERR(fbdev->pclk);
        goto out1;
    }


    //clk_disable_unprepare(fbdev->pclk);

    /*初始化info变量*/
    ret = vdmafb_init_fbinfo(fbdev, &vmode);
    if(ret)
    {
        dev_err(&pdev->dev, "Failed to initialize fb_info\n");
        goto out1;
    }

    ret = fb_alloc_cmap(&info->cmap, 256, 0); //分配调色板
    if(ret<0)
    {
        dev_err(&pdev->dev, "Failed to allocate cmap\n");
        goto out2;
    }

    info->pseudo_palette = devm_kzalloc(&pdev->dev, 16 * sizeof(u32), GFP_KERNEL);  //伪调色板
    if(!info->pseudo_palette)
    {
        dev_err(&pdev->dev, "Failed to allocate pseudo_palette\n");
        ret = -ENOMEM;
        goto out3;
    }

    // /*设置LCD像素时钟、使能时钟*/
    // ret = clk_set_rate(fbdev->pclk,PICOS2KHZ(info->var.pixclock)*1000);
    // ret = clk_prepare_enable(fbdev->pclk);
    // dev_info(&pdev->dev, "lcd_pclk frequency: %lu Hz\n", clk_get_rate(fbdev->pclk));

    msleep(5);
    /*初始化VTC*/
    ret = vdmafb_init_vtc(fbdev, &vmode);
    if(ret)
    {
        dev_err(&pdev->dev, "Failed to initialize VTC\n");
        goto out3;
    }

    /*初始化VDMA*/
    ret = vdmafb_init_vdma(fbdev);
    if(ret)
    {
        dev_err(&pdev->dev, "Failed to initialize VDMA\n");
        goto out5;
    }

    /*注册framebuffer设备*/
    ret = register_framebuffer(info);
    if(ret)
    {
        dev_err(&pdev->dev, "Failed to register framebuffer\n");
        goto out6;
    }
    platform_set_drvdata(pdev, fbdev); //保存私有数据
    dev_info(&pdev->dev, "Xilinx VDMA Framebuffer driver probed\n");

    return 0;

out6:
    dmaengine_terminate_all(fbdev->vdma);   //终止VDMA通道数据传输
    dma_release_channel(fbdev->vdma);       //释放VDMA通道
out5:
    xvtc_generator_stop(fbdev->vtc);        //停止VTC生成器

// out4:
//     clk_disable_unprepare(fbdev->pclk);     //关闭像素时钟
out3:
    fb_dealloc_cmap(&info->cmap);           //释放调色板
out2:
    dma_free_wc(&pdev->dev, PAGE_ALIGN(info->screen_size), info->screen_base, info->fix.smem_start);  //释放显存
out1:
    framebuffer_release(info);              //释放framebuffer设备
    return ret;

}

static int vdmafb_remove(struct platform_device *pdev)
{
    struct xilinx_vdmafb_dev *fbdev = platform_get_drvdata(pdev);
    struct fb_info *info = fbdev->fb_info;


    unregister_framebuffer(info);   //注销framebuffer设备
    dmaengine_terminate_all(fbdev->vdma);  //终止VDMA通道数据传输
    dma_release_channel(fbdev->vdma);      //释放VDMA通道
    xvtc_generator_stop(fbdev->vtc);       //停止VTC生成器
    //clk_disable_unprepare(fbdev->pclk);    //关闭像素时钟
    fb_dealloc_cmap(&info->cmap);          //释放调色板
    dma_free_wc(&pdev->dev, PAGE_ALIGN(info->screen_size), info->screen_base, info->fix.smem_start);  //释放显存
    framebuffer_release(info);             //释放framebuffer设备

    return 0;
}

static void vdmafb_shutdown(struct platform_device *pdev)
{

    struct xilinx_vdmafb_dev *fbdev = platform_get_drvdata(pdev);
    xvtc_generator_stop(fbdev->vtc);       //停止VTC生成器
    //clk_disable_unprepare(fbdev->pclk);    //关闭像素时钟
}


static const struct of_device_id vdmafb_of_match_table[] = {
    { .compatible = "xilinx,vdmafb" },
    { },
};

MODULE_DEVICE_TABLE(of, vdmafb_of_match_table);

static struct platform_driver xilinx_vdmafb_driver = {
    .driver = {
        .name = "xilinx-vdmafb",
        .of_match_table = vdmafb_of_match_table,
    },
    .probe = vdmafb_probe,
    .remove = vdmafb_remove,
    .shutdown = vdmafb_shutdown,
};

module_platform_driver(xilinx_vdmafb_driver);

MODULE_AUTHOR("LVD");
MODULE_DESCRIPTION("Framebuffer driver for Xilinx VDMA IP Core");
MODULE_LICENSE("GPL");

