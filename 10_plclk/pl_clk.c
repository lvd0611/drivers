#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/clk-provider.h>
struct test_clk_dev {
    struct clk *pclk;  // 指向测试的时钟
    struct platform_device *pdev;
};

static int test_clk_probe(struct platform_device *pdev)
{
    struct test_clk_dev *tdev;
    struct clk *parent_clk;
    unsigned long rate;
    int ret;

    dev_info(&pdev->dev, "Probing test clock driver\n");

    /* 分配测试设备结构体 */
    tdev = devm_kzalloc(&pdev->dev, sizeof(*tdev), GFP_KERNEL);
    if (!tdev)
        return -ENOMEM;

    tdev->pdev = pdev;
    platform_set_drvdata(pdev, tdev);

    /* 获取时钟 */
    tdev->pclk = devm_clk_get(&pdev->dev, "test_pclk");
    if (IS_ERR(tdev->pclk)) {
        dev_err(&pdev->dev, "Failed to get test_pclk\n");
        return PTR_ERR(tdev->pclk);
    }
    dev_info(&pdev->dev, "Clock acquired successfully\n");

    /* 打印时钟当前频率 */
    rate = clk_get_rate(tdev->pclk);
    dev_info(&pdev->dev, "Initial clock rate: %lu Hz\n", rate);

    /* 获取父时钟信息 */
    parent_clk = clk_get_parent(tdev->pclk);
    if (parent_clk) {
        dev_info(&pdev->dev, "Parent clock name: %s\n", __clk_get_name(parent_clk));
        dev_info(&pdev->dev, "Parent clock rate: %lu Hz\n", clk_get_rate(parent_clk));
    } else {
        dev_warn(&pdev->dev, "Parent clock not found\n");
    }

    /* 设置时钟频率 */
    rate = 71103000;  // 设置为测试频率
    ret = clk_set_rate(tdev->pclk, rate);
    if (ret) {
        dev_err(&pdev->dev, "Failed to set clock rate to %lu Hz\n", rate);
        return ret;
    }

    /* 使能时钟 */
    ret = clk_prepare_enable(tdev->pclk);
    if (ret) {
        dev_err(&pdev->dev, "Failed to enable clock\n");
        return ret;
    }

    /* 验证时钟启用后的频率 */
    rate = clk_get_rate(tdev->pclk);
    dev_info(&pdev->dev, "Clock rate after enabling: %lu Hz\n", rate);

    /* 确保时钟在启用状态 */
    if (__clk_is_enabled(tdev->pclk))
        dev_info(&pdev->dev, "Clock is enabled\n");
    else
        dev_warn(&pdev->dev, "Clock is NOT enabled\n");


    return 0;
}

static int test_clk_remove(struct platform_device *pdev)
{
    struct test_clk_dev *tdev = platform_get_drvdata(pdev);

    /* 确保时钟被禁用 */
    if (__clk_is_enabled(tdev->pclk)) {
        clk_disable_unprepare(tdev->pclk);
        dev_info(&pdev->dev, "Clock disabled during remove\n");
    }

    return 0;
}

static const struct of_device_id test_clk_dt_ids[] = {
    { .compatible = "custom,test-clk" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, test_clk_dt_ids);

static struct platform_driver test_clk_driver = {
    .driver = {
        .name = "test-clk",
        .of_match_table = test_clk_dt_ids,
    },
    .probe = test_clk_probe,
    .remove = test_clk_remove,
};

module_platform_driver(test_clk_driver);

MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Test Clock Driver");
MODULE_LICENSE("GPL");
