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
const int piece_len = 253;     
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
        char tmp[2];
        tmp[1] = SHAKE_1;
        tmp[0] = checksum(tmp + 1, 1);
        sendto(client, tmp, 2, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
        int begintime = clock();
        char recv[2];
        int lengthmp = sizeof(clientAddr);
        int fail_send = 0;
        while (recvfrom(client, recv, 2, 0, (sockaddr*)&serverAddr, &lengthmp) == SOCKET_ERROR)
            if (clock() - begintime > TIMEOUT) {//已超时
                fail_send = 1;
                break;
            }
        //接受shake_2并校验
        if (fail_send == 0 && checksum(recv, 2) == 0 && recv[1] == SHAKE_2) {
            {
                //发送shake_3
                tmp[1] = SHAKE_3;
                tmp[0] = checksum(tmp + 1, 1);
                //cout << "shake2校验和为：" << int(tmp[0]);
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
        char tmp[2];//发送数据缓存区
        tmp[1] = WAVE_1;//第二位记录挥手的字段
        tmp[0] = checksum(tmp + 1, 1);//第一位记录校验和
        //cout << "wave1校验和为：" << int(tmp[0]);
        sendto(client, tmp, 2, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));//发送到服务器端
        int begintime = clock();
        char recv[2];
        int lengthmp = sizeof(serverAddr);
        int fail_send = 0;//标识是否发送失败
        while (recvfrom(client, recv, 2, 0, (sockaddr*)&serverAddr, &lengthmp) == SOCKET_ERROR)
            if (clock() - begintime > TIMEOUT) {//接收已经超时
                fail_send = 1;//发送失败
                tot_fail++;//失败次数加一
                break;
            }
        //接受wave_2并校验
        if (fail_send == 0 && checksum(recv, 2) == 0 && recv[1] == WAVE_2)
            break;
        else {//未收到
            
            if (tot_fail == 3) {
                printf("挥手失败...");
                break;
            }
            continue;
        }
    }
}


bool send_package(char* message, int length, int order, int last = 0) {//分片发包
    if (length > piece_len) {//单片长度过大
        return false;
    }
    if (last == false && length != piece_len) {//不是最后一个包也不是单个片长度
        return false;
    }
    char* tmp;
    int tmp_len;

    if (last) {//是最后一个包
        tmp = new char[length + 4];//分配缓冲区
        tmp[1] = LAST_PACK;
        tmp[2] = order;//序列号
        tmp[3] = length;//多一个长度的存储
        for (int i = 4; i < length + 4; i++)
            tmp[i] = message[i - 4];//存入内容
        tmp[0] = checksum(tmp + 1, length + 3);//第一位存入校验和
        tmp_len = length + 4;//还是记录一下长度
    }
    else {
        tmp = new char[length + 3];
        tmp[1] = NOTLAST_PACK;//不是最后一个包
        tmp[2] = order;
        for (int i = 3; i < length + 3; i++)
            tmp[i] = message[i - 3];
        tmp[0] = checksum(tmp + 1, length + 2);
        //cout << "bushilastbao校验和为：" << int(tmp[0]);
        tmp_len = length + 3;
    }
    sendto(client, tmp, tmp_len, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));//将单片的信息发送过去
    return true;
}



//序列号编号256 记录是否在窗口内
bool in_list[UCHAR_MAX + 1];  
void send_message(char* message, int length) {  
    queue<pair<int, int>> send_time_list;//存入发送出去的时间点 
    static int windowbase = 0;
    int send_count = 0;//已发送未确认    
    int nextseqnum = windowbase;  
    int send_succ_count = 0;//已发送已确认   
    int package_sum = length / piece_len + (length % piece_len != 0);//确定发包数，是否加最后一个包
    while (1) {

        //先判断是否已全部发送成功
        if (send_succ_count == package_sum)
            break;
        //在发送窗口未填满时，边发送边进行接收检测，查看对方的是否有累计确认状态码发送过来。
        if (send_time_list.size() < WINDOW_SIZE && send_count != package_sum) {
            send_package(message + send_count * piece_len,
                send_count == package_sum - 1 ? length - (package_sum - 1) * piece_len : piece_len,//是最后一个包长度取前面那个
                nextseqnum % ((int)UCHAR_MAX + 1),//序列号
                send_count == package_sum - 1);//是否是最后一个包

            //发送后放入窗口，开始计时
            send_time_list.push(make_pair(clock(), nextseqnum % ((int)UCHAR_MAX + 1)));
            //并记录在窗口里面的序列号，进入窗口的记为1
            in_list[nextseqnum % ((int)UCHAR_MAX + 1)] = 1;
            nextseqnum++;
            send_count++;
        }
        //使用了while循环来回调换recv和send进行实现，避免了使用线程导致的繁琐。
        
        char recv[3];
        int lengthmp = sizeof(serverAddr);
        //窗口满了就等待ack确认后窗口减小，才能继续发送
        if (recvfrom(client, recv, 3, 0, (sockaddr*)&serverAddr, &lengthmp) != SOCKET_ERROR && checksum(recv, 3) == 0 &&
            recv[1] == ACK && in_list[(unsigned char)recv[2]]) {//recv2是序列号，接收ACK

            //一个累计确认状态的序号高于当前的windowbase序号的包，则进行发送窗口的滑动，
            while (send_time_list.front().second < (unsigned char)recv[2]) {
                send_succ_count++;//已确认
                windowbase++;//窗口移动
                in_list[send_time_list.front().second] = 0;//将该包的序列号移出窗口，记为0
                send_time_list.pop();//删除窗口时间序列第一个元素
            }
            //未超时收到包，则需要弹出队列队首，即更新计时器。
            in_list[send_time_list.front().second] = 0;//对应的序列号，不在窗口内，记为0
            send_succ_count++;
            windowbase++;
            send_time_list.pop();//删除第一个元素
        }
        else {
            //超时后进行回退重发，并且清空队列
            if (clock() - send_time_list.front().first > TIMEOUT) {//当前clock()判断队首的包是否发送超时
                nextseqnum = windowbase;//回退到windowbase
                send_count -= send_time_list.size();//减去现在队列中的元素个数，缩小窗口等待的数据包
                //清空计时器。
                while (!send_time_list.empty()) 
                    send_time_list.pop();
            }
        }
        
        if (windowbase % 100 == 0)
            printf("已经发送第%d个数据包，此文件总共已经发送%.2f%%\n",windowbase, (float)windowbase / package_sum * 1000);
    }
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
    //设置非阻塞
    int time_out = 1;//1ms超时
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
    printf("请输入发送窗口大小：\n");
    WINDOW_SIZE=1;
    WINDOW_SIZE %= UCHAR_MAX; //防止窗口大小大于序号域长度
    printf("连接建立中...\n");
    shake_hand();
    printf("连接建立完成。 \n正在发送信息...\n");
    clock_t start, end,time;
    send_message((char*)(filename.c_str()), filename.length());
    printf("文件名发送完毕，正在发送文件内容...\n");
    start = clock();
    send_message(buffer, len);
    end = clock();
    time = (end - start) / CLOCKS_PER_SEC;
    printf("发送的文件bytes:%d\n", len);
    cout << "传输时间为：" <<setw(10)<< (double)time << "s" << endl;
    cout << "平均吞吐率为：" << len*8/1000  / (double)time << "kbps" << endl;
    printf("文件内容发送完毕。\n 开始断开连接...\n");
    wave_hand();
    printf("连接已断开。\n");
    printf("继续发送输入1，退出0\n");
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