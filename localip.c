1. 通过gethostname()和gethostbyname()

#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main() {
    char hname[128];
    struct hostent *hent;
    int i;

    gethostname(hname, sizeof(hname));

    //hent = gethostent();
    hent = gethostbyname(hname);

    printf("hostname: %s/naddress list: ", hent->h_name);
    for(i = 0; hent->h_addr_list[i]; i++) {
        printf("%s/t", inet_ntoa(*(struct in_addr*)(hent->h_addr_list[i])));
    }
    return 0;
}


2. 通过枚举网卡，API接口可查看man 7 netdevice
/*代码来自StackOverflow: http://stackoverflow.com/questions/212528/linux-c-get-the-ip-address-of-local-computer */
#include <stdio.h>      
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h> 
#include <string.h> 
#include <arpa/inet.h>

int main (int argc, const char * argv[]) {
    struct ifaddrs * ifAddrStruct=NULL;
    void * tmpAddrPtr=NULL;

    getifaddrs(&ifAddrStruct);

    while (ifAddrStruct!=NULL) {
        if (ifAddrStruct->ifa_addr->sa_family==AF_INET) { // check it is IP4
            // is a valid IP4 Address
            tmpAddrPtr=&((struct sockaddr_in *)ifAddrStruct->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            printf("%s IP Address %s/n", ifAddrStruct->ifa_name, addressBuffer); 
        } else if (ifAddrStruct->ifa_addr->sa_family==AF_INET6) { // check it is IP6
            // is a valid IP6 Address
            tmpAddrPtr=&((struct sockaddr_in *)ifAddrStruct->ifa_addr)->sin_addr;
            char addressBuffer[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
            printf("%s IP Address %s/n", ifAddrStruct->ifa_name, addressBuffer); 
        } 
        ifAddrStruct=ifAddrStruct->ifa_next;
    }
    return 0;
}


3. 打开一个对外界服务器的网络连接，通过getsockname()反查自己的IP
int fd = socket (AF_INET, SOCK_DGRAM, 0);
    if ( fd >= 0)
    {
        struct ifreq buf[512];
        struct ifconf ifc; 
        ifc.ifc_len = sizeof buf;
	ifc.ifc_buf = (caddr_t) buf;

        if (!ioctl (fd, SIOCGIFCONF, (char *) &ifc)) //ioctl.h    
	{
            int intrface = ifc.ifc_len / sizeof (struct ifreq);
            while (intrface-- > 0)
            {
                if (!(ioctl (fd, SIOCGIFCONF, (char *) &ifc)))    //                if (!(ioctl (fd, SIOCGIFCONF, (char *) &buf[intrface]))) 
                {
                    std::string temp = buf[intrface].ifr_name;          //device name
                    if (temp.compare("lo") == 0 || temp.find("lo:") != std::string::npos)             //过滤lo和lvs
                        continue;
                    temp = inet_ntoa( ((struct sockaddr_in*)(&buf[intrface].ifr_addr))->sin_addr);      //device ip
                    if (temp.find("127.0.0") != std::string::npos )
                        continue;
                    if ( find( ifrip.begin(), ifrip.end(), temp ) == ifrip.end() ){
                        //
                    }
                }
            }
        }
        close (fd);
    }










