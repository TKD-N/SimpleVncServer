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
static uint8_t frame_buffer[800 * 480 * 4] = {0};

int task(void);

static bool vncConnect(int fd);
static bool vncReceive(int fd);

static bool updateFrameBuffer(void);
static bool sendFrameBuffer(int fd);

static bool sockWrite(int fd, const void *buf, size_t n);
static bool sockRead(int fd, void *buf, size_t n);
static bool sockSkip(int fd, size_t n);

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
        printf("server waiting\n");

        /* 接続待機(戻り値はクライアントソケットのFD) */
        client_sockfd = accept(server_sockfd ,
                               (struct sockaddr *)&client_address ,
                               &client_len);

        printf("socket accept!\n");
        
        if(!vncConnect(client_sockfd)) {
            close(client_sockfd);
            continue;
        }
        
        vncReceive(client_sockfd);
 
        /* クライアントソケットのクローズ */
        close(client_sockfd);
    }
    
    /* サーバーソケットのクローズ */
    close(server_sockfd);
    return 0;
}


static bool
vncConnect(int fd)
{
    const char  *write_str;
    size_t      write_size;
    uint32_t    security_type;
    
    /* ProtocolVersion */
    /* プロトコルバージョンを通知 */
    write_str   = "RFB 003.003\n";
    write_size  = strlen(write_str);
    /* 書き込み */
    if(!sockWrite(fd, write_str, 12)) {
        printf("write error in l.%d\n", __LINE__);
        return false;
    }
    printf("write version\n");
    
    /* 読み込み */
    memset(sock_buffer, 0x00, sizeof(sock_buffer));
    if(!sockRead(fd, sock_buffer, 12)) {
        printf("read error in l.%d\n", __LINE__);
        return false;
    }
    printf("read version\n");
    /* バージョンチェック */
    sock_buffer[13] = '\0'; // 14byte目にNULL文字を挿入
    if(strcmp((char *)sock_buffer, "RFB 003.003\n") != 0) {
        /* 非サポート */
        security_type = htonl(0);   // Invalid: 接続失敗
        if(!sockWrite(fd, &security_type, sizeof(security_type))) {
            printf("write error in l.%d\n", __LINE__);
        }
        /* 理由を通知 */
        const char *reason_str = "Specified version is not supported.\n";
        uint32_t reason_length = strlen(reason_str);
        write_size  = sizeof(reason_length) + reason_length;
        memcpy(sock_buffer, &reason_length, sizeof(reason_length));
        memcpy(&sock_buffer[sizeof(reason_length)], reason_str, reason_length);
        if(!sockWrite(fd, sock_buffer, sizeof(write_size))) {
            printf("write error in l.%d\n", __LINE__);
        }
        /* 接続失敗 */
        return false;
    }

    /* Security */
    /* セキュリティタイプを通知 */
    security_type = htonl(1);   // None: 認証不要
    if(!sockWrite(fd, &security_type, sizeof(security_type))) {
        printf("write error in l.%d\n", __LINE__);
        return false;
    }
    
    /* ClientInit */
    /* クライアントから初期化メッセージを受信 */
    memset(sock_buffer, 0x00, sizeof(sock_buffer));
    if(!sockRead(fd, sock_buffer, 1)) {
        printf("read error in l.%d\n", __LINE__);
        return false;
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
    if(!sockWrite(fd, sock_buffer, server_init_str - sock_buffer)) {
        printf("write error in l.%d\n", __LINE__);
        return false;
    }

    return true;
}

static bool
vncReceive(int fd)
{
    uint16_t number_of_encodings;
    uint32_t client_cut_length;
    int i;
    while(1) {
        uint8_t message_type;
        printf("wait receive\n");
        /* 受信(message-type読込) */
        if(!sockRead(fd, &message_type, sizeof(message_type))) {
            printf("read error in l.%d\n", __LINE__);
            return false;
        }
        switch(message_type) {
            case 3: /* FramebufferUpdateRequest */
                printf("receive FramebufferUpdateRequest.\n");
                if(!sockSkip(fd, 9)) {
                    return false;
                }
                updateFrameBuffer();
                if(!sendFrameBuffer(fd)) {
                    return false;
                }
                break;
            case 0: /* SetPixelFormat */
                printf("receive SetPixelFormat.\n");
                if(!sockSkip(fd, 19)) {
                    return false;
                }
                break;
            case 2: /* SetEncodings */
                printf("receive SetEncodings.\n");
                if(!sockSkip(fd, 1)) {
                    return false;
                }
                if(!sockRead(fd, &number_of_encodings, sizeof(number_of_encodings))) {
                    return false;
                }
                number_of_encodings = ntohs(number_of_encodings);
                for(i = 0; i < number_of_encodings; i++) {
                    if(!sockSkip(fd, 4)) {
                        return false;
                    }
                }
                break;
            case 4: /* KeyEvent */
                printf("receive KeyEvent.\n");
                if(!sockSkip(fd, 7)) {
                    return false;
                }
                break;
            case 5: /* PointerEvent */
                printf("receive PointerEvent.\n");
                if(!sockSkip(fd, 5)) {
                    return false;
                }
                break;
            case 6: /* ClientCutText */
                printf("receive ClientCutText.\n");
                if(!sockSkip(fd, 3)) {
                    return false;
                }
                if(!sockRead(fd, &client_cut_length, sizeof(client_cut_length))) {
                    return false;
                }
                client_cut_length = ntohl(client_cut_length);
                if(!sockSkip(fd, client_cut_length)) {
                    return false;
                }
                break;
            default:
                printf("receive non-supported message(=%u).\n", message_type);
                return false;
        }
    }
    return true;
}

static int state = 0;
static bool
updateFrameBuffer(void)
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    int x, y;
    int line_offset;
    int offset;
    
    memset(frame_buffer, 0x00, sizeof(frame_buffer));
    switch(state % 4) {
        case 0:
            red = 100;  green = 100;    blue = 0;
            break;
        case 1:
            red = 255;  green = 0;      blue = 0;
            break;
        case 2:
            red = 255;  green = 255;    blue = 0;
            break;
        case 3:
        default:
            red = 255;  green = 255;    blue = 255;
            break;
    }
    for(y = 0; y < 480; y++) {
        line_offset = y * 800 * 4;
        for(x = 0; x < 800; x++) {
            offset = line_offset + x * 4;
            frame_buffer[offset]        = red;
            frame_buffer[offset + 1]    = green;
            frame_buffer[offset + 2]    = blue;
        }
    }
    state++;
    return true;
}

static bool
sendFrameBuffer(int fd)
{
    uint8_t message_type = 0;
    uint8_t padding = 0;
    uint16_t number_of_rectangles = htons(1);

    uint8_t header_buffer[4];
    uint8_t rect_header_buffer[12];
    
    uint8_t *buffer = NULL;
    
    buffer = header_buffer;
    memcpy(buffer, &message_type, sizeof(message_type));
    buffer += sizeof(message_type);
    memcpy(buffer, &padding, sizeof(padding));
    buffer += sizeof(padding);
    memcpy(buffer, &number_of_rectangles, sizeof(number_of_rectangles));
    buffer += sizeof(number_of_rectangles);

    if(!sockWrite(fd, header_buffer, sizeof(header_buffer))) {
        printf("write error in l.%d\n", __LINE__);
        return false;
    }
    uint16_t x_position = htons(0);
    uint16_t y_position = htons(0);
    uint16_t width = htons(800);
    uint16_t height = htons(480);
    uint32_t encoding_type = htonl(0);
    
    buffer = rect_header_buffer;
    memcpy(buffer, &x_position, sizeof(x_position));
    buffer += sizeof(x_position);
    memcpy(buffer, &y_position, sizeof(y_position));
    buffer += sizeof(y_position);
    memcpy(buffer, &width, sizeof(width));
    buffer += sizeof(width);
    memcpy(buffer, &height, sizeof(height));
    buffer += sizeof(height);
    memcpy(buffer, &encoding_type, sizeof(encoding_type));
    buffer += sizeof(encoding_type);
    if(!sockWrite(fd, rect_header_buffer, sizeof(rect_header_buffer))) {
        printf("write error in l.%d\n", __LINE__);
        return false;
    }

    if(!sockWrite(fd, frame_buffer, sizeof(frame_buffer))) {
        printf("write error in l.%d\n", __LINE__);
        return false;
    }
    
    return true;
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
        if(read_size > 0) {
            work_buf += read_size;
            remain_size -= read_size;
        } else if(read_size == 0) {
            printf("read fin\n");
            return false;
        } else {
            printf("read error(=%lo)\n", read_size);
            return false;
        }
    }
    return true;
}

static bool
sockSkip(int fd, size_t n)
{
    bool result = false;
    uint8_t *buffer = NULL;
    buffer = (uint8_t *)malloc(n);
    if(!buffer) {
        printf("alloc error in l%d\n", __LINE__);
        return false;
    }
    result = sockRead(fd, buffer, n);
    free(buffer);
    return result;
}