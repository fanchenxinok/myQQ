#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
 
#define MAXINTERFACES 16    /* ���ӿ��� */
 
int fd;         /* �׽��� */
int if_len;     /* �ӿ����� */
struct ifreq buf[MAXINTERFACES];    /* ifreq�ṹ���� */
struct ifconf ifc;                  /* ifconf�ṹ */
 
int main(int argc, char** argv)
{
    /* ����IPv4��UDP�׽���fd */
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
        perror("socket(AF_INET, SOCK_DGRAM, 0)");
        return -1;
    }
 
    /* ��ʼ��ifconf�ṹ */
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = (caddr_t) buf;
 
    /* ��ýӿ��б� */
    if (ioctl(fd, SIOCGIFCONF, (char *) &ifc) == -1){
        perror("SIOCGIFCONF ioctl");
        return -1;
    }
 
    if_len = ifc.ifc_len / sizeof(struct ifreq); /* �ӿ����� */
    printf("interface number: %d\n", if_len);

    int n = 0;
    while (if_len-- > 0) /* ����ÿ���ӿ� */
    {
        n++;
        printf("(%d)interface name: %s\n", n, buf[if_len].ifr_name); /* �ӿ����� */
 
        /* ��ýӿڱ�־ */
        if (!(ioctl(fd, SIOCGIFFLAGS, (char *) &buf[if_len]))){
            /* �ӿ�״̬ */
            if (buf[if_len].ifr_flags & IFF_UP){
                printf("interface status: UP\n");
            }
            else{
                printf("interface status: DOWN\n");
            }
        }
        else{
            char str[256];
            sprintf(str, "SIOCGIFFLAGS ioctl %s", buf[if_len].ifr_name);
            perror(str);
        }
 
 
        /* IP��ַ */
        if (!(ioctl(fd, SIOCGIFADDR, (char *) &buf[if_len]))){
            printf("IP address:%s\n",
                    (char*)inet_ntoa(((struct sockaddr_in*) (&buf[if_len].ifr_addr))->sin_addr));
        }
        else{
            char str[256];
            sprintf(str, "SIOCGIFADDR ioctl %s", buf[if_len].ifr_name);
            perror(str);
        }
 
        /* �������� */
        if (!(ioctl(fd, SIOCGIFNETMASK, (char *) &buf[if_len]))){
            printf("subnetwork mask:%s\n",
                    (char*)inet_ntoa(((struct sockaddr_in*) (&buf[if_len].ifr_addr))->sin_addr));
        }
        else{
            char str[256];
            sprintf(str, "SIOCGIFADDR ioctl %s", buf[if_len].ifr_name);
            perror(str);
        }
 
        /* �㲥��ַ */
        if (!(ioctl(fd, SIOCGIFBRDADDR, (char *) &buf[if_len]))){
            printf("brocast address:%s\n",
                    (char*)inet_ntoa(((struct sockaddr_in*) (&buf[if_len].ifr_addr))->sin_addr));
        }
        else{
            char str[256];
            sprintf(str, "SIOCGIFADDR ioctl %s", buf[if_len].ifr_name);
            perror(str);
        }
 
        /*MAC��ַ */
        if (!(ioctl(fd, SIOCGIFHWADDR, (char *) &buf[if_len]))){
            printf("MAC address:%02x:%02x:%02x:%02x:%02x:%02x\n",
                    (unsigned char) buf[if_len].ifr_hwaddr.sa_data[0],
                    (unsigned char) buf[if_len].ifr_hwaddr.sa_data[1],
                    (unsigned char) buf[if_len].ifr_hwaddr.sa_data[2],
                    (unsigned char) buf[if_len].ifr_hwaddr.sa_data[3],
                    (unsigned char) buf[if_len].ifr_hwaddr.sa_data[4],
                    (unsigned char) buf[if_len].ifr_hwaddr.sa_data[5]);
        }
        else{
            char str[256];
            sprintf(str, "SIOCGIFHWADDR ioctl %s", buf[if_len].ifr_name);
            perror(str);
        }
    }//�Cwhile end
 
    //�ر�socket
    close(fd);
    return 0;
}

