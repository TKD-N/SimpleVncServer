#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdbool.h>

static uint8_t sock_buffer[100] = {0};

int task(void);
static bool sockWrite(int fd, const void *buf, size_t n);
static bool sockRead(int fd, void *buf, size_t n);

int main(int argc, char **argv) {
    return task();
}

int task(void)
{
    int server_sockfd , client_sockfd ;
    socklen_t server_len , client_len ;
    struct sockaddr_in server_address ;
    struct sockaddr_in client_address ;

    printf("Hello World!\n");

    /* ソケット生成 */
    server_sockfd = socket(AF_INET,SOCK_STREAM,0);
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(5900);

    server_len = sizeof(server_address);
    /* バインド */
    bind(server_sockfd , (struct sockaddr *)&server_address , server_len);

    /* 接続待ち設定(キュー長: 5) */
    listen(server_sockfd , 5);
    
    while(1) {
        const char  *write_str;
        size_t      write_size;
        uint32_t    security_type;

        printf("server waiting\n");

        /* 接続待機(戻り値はクライアントソケットのFD) */
        client_sockfd = accept(server_sockfd ,
                               (struct sockaddr *)&client_address ,
                               &client_len);

        printf("socket accept!\n");
        
        /* ProtocolVersion */
        /* プロトコルバージョンを通知 */
        write_str   = "RFB 003.003\n";
        write_size  = strlen(write_str);
        /* 書き込み */
        sockWrite(client_sockfd, write_str, 12);
        printf("write version\n");
        
        /* 読み込み */
        memset(sock_buffer, 0x00, sizeof(sock_buffer));
        if(!sockRead(client_sockfd, sock_buffer, 12)) {
            printf("read error in l.%d\n", __LINE__);
            continue;
        }
        printf("read version\n");
        /* バージョンチェック */
        sock_buffer[13] = '\0'; // 14byte目にNULL文字を挿入
        if(strcmp((char *)sock_buffer, "RFB 003.003\n") != 0) {
            /* 非サポート */
            security_type = htonl(0);   // Invalid: 接続失敗
            sockWrite(client_sockfd, &security_type, sizeof(security_type));
            /* 理由を通知 */
            const char *reason_str = "Specified version is not supported.\n";
            uint32_t reason_length = strlen(reason_str);
            write_size  = sizeof(reason_length) + reason_length;
            memcpy(sock_buffer, &reason_length, sizeof(reason_length));
            memcpy(&sock_buffer[sizeof(reason_length)], reason_str, reason_length);
            sockWrite(client_sockfd, sock_buffer, sizeof(write_size));
            /* 接続を閉じる */
            close(client_sockfd);
            continue;
        }

        /* Security */
        /* セキュリティタイプを通知 */
        security_type = htonl(1);   // None: 認証不要
        sockWrite(client_sockfd, &security_type, sizeof(security_type));
        
        /* ClientInit */
        /* クライアントから初期化メッセージを受信 */
        memset(sock_buffer, 0x00, sizeof(sock_buffer));
        if(!sockRead(client_sockfd, sock_buffer, 1)) {
            printf("read error in l.%d\n", __LINE__);
            continue;
        }
        /* 共有フラグの有無は無視 */

        /* ServerInit */
        memset(sock_buffer, 0x00, sizeof(sock_buffer));
        uint8_t *server_init_str = sock_buffer;
        uint16_t width = htons(800), height = htons(480);
        memcpy(server_init_str, &width, sizeof(width));
        server_init_str += sizeof(width);
        memcpy(server_init_str, &height, sizeof(height));
        server_init_str += sizeof(height);
        
        /* PIXEL_FORMAT */
        uint8_t bits_per_pixel = 32;
        uint8_t depth = 32;
        uint8_t big_endian_flag = 0;
        uint8_t true_color_flag = 1;
        uint16_t red_max = htons(255);
        uint16_t green_max = htons(255);
        uint16_t blue_max = htons(255);
        uint8_t red_shift = 24;
        uint8_t green_shift = 16;
        uint8_t blue_shift = 8;
        memcpy(server_init_str, &bits_per_pixel, sizeof(bits_per_pixel));
        server_init_str += sizeof(bits_per_pixel);
        memcpy(server_init_str, &depth, sizeof(depth));
        server_init_str += sizeof(depth);
        memcpy(server_init_str, &big_endian_flag, sizeof(big_endian_flag));
        server_init_str += sizeof(big_endian_flag);
        memcpy(server_init_str, &true_color_flag, sizeof(true_color_flag));
        server_init_str += sizeof(true_color_flag);
        memcpy(server_init_str, &red_max, sizeof(red_max));
        server_init_str += sizeof(red_max);
        memcpy(server_init_str, &green_max, sizeof(green_max));
        server_init_str += sizeof(green_max);
        memcpy(server_init_str, &blue_max, sizeof(blue_max));
        server_init_str += sizeof(blue_max);
        memcpy(server_init_str, &red_shift, sizeof(red_shift));
        server_init_str += sizeof(red_shift);
        memcpy(server_init_str, &green_shift, sizeof(green_shift));
        server_init_str += sizeof(green_shift);
        memcpy(server_init_str, &blue_shift, sizeof(blue_shift));
        server_init_str += sizeof(blue_shift);
        
        /* デスクトップの名前 */
        write_str   = "Simple VNC";
        write_size  = strlen(write_str);
        memcpy(server_init_str, &write_size, sizeof(write_size));
        server_init_str += sizeof(write_size);
        memcpy(server_init_str, write_str, write_size);
        server_init_str += write_size;
        
        /* ServerInit を送信(サイズはアドレスの引き算) */
        sockWrite(client_sockfd, sock_buffer, server_init_str - sock_buffer);
        
        sleep(5);
 
        /* クライアントソケットのクローズ */
        close(client_sockfd);
    }
    
    /* サーバーソケットのクローズ */
    close(server_sockfd);
    return 0;
}

static bool
sockWrite(int fd, const void *buf, size_t n)
{
    size_t remain_size = n;
    size_t write_size;

    const void *work_buf = buf;
    
    while(remain_size > 0) {
        write_size = write(fd, work_buf, remain_size);
        if(write_size >= 0) {
            work_buf += write_size;
            remain_size -= write_size;
        } else {
            printf("write error(=%lo)\n", write_size);
            return false;
        }
    }
    return true;
}

static bool
sockRead(int fd, void *buf, size_t n)
{
    size_t remain_size = n;
    size_t read_size;

    void *work_buf = buf;
    
    while(remain_size > 0) {
        read_size = read(fd, work_buf, remain_size);
        if(read_size >= 0) {
            work_buf += read_size;
            remain_size -= read_size;
        } else {
            printf("read error(=%lo)\n", read_size);
            return false;
        }
    }
    return true;
}
