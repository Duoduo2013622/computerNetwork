#pragma comment(lib, "Ws2_32.lib")

#include <iostream>
#include <winsock2.h>
#include <stdio.h>
#include <fstream>
#include <vector>
#include <string>
#include <time.h>
#include <queue>
#include <iomanip>

using namespace std;
const int Mlenx = 253;
const unsigned char ACK = 0x03;
const unsigned char NAK = 0x07;
const unsigned char LAST_PACK = 0x18;
const unsigned char NOTLAST_PACK = 0x08;
const unsigned char SHAKE_1 = 0x01;
const unsigned char SHAKE_2 = 0x02;
const unsigned char SHAKE_3 = 0x04;
const unsigned char WAVE_1 = 0x80;
const unsigned char WAVE_2 = 0x40;
int WINDOW_SIZE;
const int TIMEOUT = 5000;//毫秒
char buffer[200000000];
int len;

SOCKET client;
SOCKADDR_IN serverAddr, clientAddr;

unsigned char checksum(char* flag, int len) {//计算校验和
    if (len == 0) {
        return ~(0);
    }
    unsigned int ret = 0;
    int i = 0;
    while (len--) {
        ret += (unsigned char)flag[i++];
        if (ret & 0xFFFF0000) {
            ret &= 0xFFFF;
            ret++;
        }
    }
    return ~(ret & 0xFFFF);
}

void shake_hand() {//握手，建立连接
    while (1) {
        //发送shake_1
        char tmp[2];//发送数据缓存区
        tmp[1] = SHAKE_1;//第二位记录握手的字段
        tmp[0] = checksum(tmp + 1, 1);
        sendto(client, tmp, 2, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
        int begintime = clock();  //开始计时
        char recv[2];
        int last_lenmp = sizeof(clientAddr);
        int fail_send = 0;     //记录失败次数
        while (recvfrom(client, recv, 2, 0, (sockaddr*)&serverAddr, &last_lenmp) == SOCKET_ERROR)
            if (clock() - begintime > TIMEOUT) {//已超时 退出重新shake_1
                fail_send = 1;
                break;
            }
        //收到shake_2并校验
        if (fail_send == 0 && checksum(recv, 2) == 0 && recv[1] == SHAKE_2) {
            {
                //发送shake_3
                tmp[1] = SHAKE_3;
                tmp[0] = checksum(tmp + 1, 1);
                sendto(client, tmp, 2, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
                break;
            }
        }
    }
}

void wave_hand() {//进行挥手，断开连接
    int tot_fail = 0;//记录失败次数
    while (1) {
        //发送wave_1
        char tmp[2];
        tmp[1] = WAVE_1;//第二位记录挥手的字段
        tmp[0] = checksum(tmp + 1, 1);//第一位记录校验和
        sendto(client, tmp, 2, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));//发送到服务器端
        int begintime = clock();
        char recv[2];
        int last_lenmp = sizeof(serverAddr);
        int fail_send = 0;//是否发送失败
        while (recvfrom(client, recv, 2, 0, (sockaddr*)&serverAddr, &last_lenmp) == SOCKET_ERROR)
            //接收已经超时失败
            if (clock() - begintime > TIMEOUT) {
                fail_send = 1;//发送失败
                tot_fail++;//失败次数加一
                break;
            }

        //接受wave_2并校验成功
        if (fail_send == 0 && checksum(recv, 2) == 0 && recv[1] == WAVE_2)
            break;
        
        //发送wave_1失败或接收wave_2失败，重发
        else {
            if (tot_fail == 3) {
                printf("挥手失败...");
                break;
            }
            continue;
        }
    }
}

//分片发包
//起始位置，长度，序列号，是否是最后一个
bool send_package(char* message, int last_len, int seq, int last = 0) {
    //单片长度过大,或不是最后一个包也不是单个片长度
    if (last_len > Mlenx && last == false && last_len != Mlenx) {
        return false;
    }

    //设置报文
    char* tmp;
    int tmp_len;
    //是最后一个包
    if (last) {
        tmp = new char[last_len + 4];//分配缓冲区
        tmp[1] = LAST_PACK;
        tmp[2] = seq;//序列号
        tmp[3] = last_len;//比常规的片多一个长度的存储
        for (int i = 4; i < last_len + 4; i++)
            tmp[i] = message[i - 4];//存入内容
        tmp[0] = checksum(tmp + 1, last_len + 3);//第一位存入校验和
        tmp_len=last_len + 4;
    }
    else {
        tmp = new char[last_len + 3];
        tmp[1] = NOTLAST_PACK;//不是最后一个包
        tmp[2] = seq;
        for (int i = 3; i < last_len + 3; i++)
            tmp[i] = message[i - 3];
        tmp[0] = checksum(tmp + 1, last_len + 2);
        tmp_len=last_len + 3;
    }
    //发送该单片的信息
    sendto(client, tmp, tmp_len, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
    return true;
}


void send_message(char* message, int last_len) {
    int begintime;  //存发送出去的时间点
    //int leave_cnt = 0;
    int has_send = 0;//已发送未确认的片数
    int nextseqnum = 0;
    int has_send_succ = 0;//已确认的片数
    int tot_package = last_len / Mlenx + (last_len % Mlenx != 0);//确定发包数
    while (1) {
        //是否已全部发送成功
        if (has_send_succ == tot_package)
            break;
        //边发送边进行接收检测，查看对方的是否有确认状态码发送过来。
        if (has_send != tot_package) {
            send_package(message + has_send * Mlenx,
                has_send == tot_package - 1 ? last_len - (tot_package - 1) * Mlenx : Mlenx,//是最后一个包长度取前面那个
                nextseqnum ,//序列号
                has_send == tot_package - 1);//是否是最后一个包
            begintime=clock();//开始计时
            nextseqnum++;
            has_send++;
        }
        //使用了while循环来回调换recv和send进行实现，避免了使用线程导致的繁琐。
        //收到更新定时器，继续发送。
        char recv[3];
        int last_lenmp = sizeof(serverAddr);

        //未超时收到确认包，更新计时器。
        if (recvfrom(client, recv, 3, 0, (sockaddr*)&serverAddr, &last_lenmp) != SOCKET_ERROR && checksum(recv, 3) == 0 &&
            recv[1] == ACK) {  //如果接收到：校验和，标志位，序列号，且校验和为0，标志位为ack 

            has_send_succ++;//已确认       
        }
        else {
            //超时后进行回退一个，重新发送
            if (clock() - begintime > TIMEOUT) {
                nextseqnum --;
                has_send --;
            }
        }
    }
    if (has_send_succ % 100 == 0)
            printf("此文件已经发送第%d个数据包\n", has_send_succ);
}


int main() {


    WSADATA wsadata;
    int error = WSAStartup(MAKEWORD(2, 2), &wsadata);
    if (error) {
        printf("init error");
        return 0;
    }
    string serverip;
    while (1) {
        printf("请输入接收方ip地址:\n");
        getline(cin, serverip);

        if (inet_addr(serverip.c_str()) == INADDR_NONE) {
            printf("ip地址不合法!\n");
            continue;
        }
        break;
    }

    int port = 11451;

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(serverip.c_str());
    client = socket(AF_INET, SOCK_DGRAM, 0);
    int time_out = 1;//设置非阻塞1ms超时
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (char*)&time_out, sizeof(time_out));
    if (client == INVALID_SOCKET) {
        printf("creat udp socket error");
        return 0;
    }

    while(1){
    string filename;
    while (1) {
        printf("请输入要发送的文件名：");
        cin >> filename;
        ifstream fin(filename.c_str(), ifstream::binary);
        if (!fin) {
            printf("文件未找到!\n");
            continue;
        }
        unsigned char t = fin.get();
        while (fin) {
            buffer[len++] = t;
            t = fin.get();
        }
        fin.close();
        break;
    }
    printf("连接建立中...\n");
    shake_hand();
    printf("握手完成，连接已建立。 \n正在发送信息...\n");
    clock_t start, end,time;
    send_message((char*)(filename.c_str()), filename.length());
    printf("文件名发送完毕。 \n正在发送文件内容...\n");
    start = clock();
    send_message(buffer, len);
    end = clock();
    time = (end - start) / CLOCKS_PER_SEC;
    printf("发送的文件共:%dbytes\n", len);
    cout << "传输时间为：" <<setw(10)<< (double)time << "s" << endl;
    cout << "平均吞吐率为：" << len*8/1000  / (double)time << "kbps" << endl;
    printf("文件内容发送完毕。\n 开始断开连接...\n");
    wave_hand();
    printf("挥手完成，连接已断开。\n");
    printf("输入0退出\n");
    int a;
    cin >> a;
    if (a==0){
        break;
    }
    }
    closesocket(client);
    WSACleanup();

    return 0;
}