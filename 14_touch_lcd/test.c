#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <linux/input.h>

int main() {
    int fd = open("/dev/input/event3", O_RDONLY);  // X 是你的触摸设备文件号
    if (fd == -1) {
        perror("Error opening device");
        return 1;
    }

    struct input_event ev;
    while (1) {
        if (read(fd, &ev, sizeof(struct input_event)) == sizeof(struct input_event)) {
            // 判断按键事件类型
            if (ev.type == EV_KEY) {
                if (ev.code == BTN_TOUCH) {  // BTN_TOUCH 按键代码
                    if (ev.value == 1) {  // 按下事件
                        printf("Touch down\n");
                    } else if (ev.value == 0) {  // 松开事件
                        printf("Touch up\n");
                    }
                }
            }
        }
    }
    close(fd);
    return 0;
}
