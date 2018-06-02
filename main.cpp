#include <iostream>
#include <getopt.h>
#include <cstdlib>
#include <ctime>
#include <string>
#include <cstring>        // bzero
#include <signal.h>
#include <sys/ioctl.h>    // ioctl
#include <net/if.h>       // ifr
#include <sys/socket.h>   // socket
#include <netinet/in.h>   // socket
#include <net/ethernet.h> // ether_header
#include <linux/if_packet.h> // struct sockaddr_ll
#include <errno.h>        // perror
#include <unistd.h>       // close
#include <iomanip>        // setw, setfill
#include <assert.h>       // assert

#define HWADDR_LEN 6
#define USERNAME_SIZ 10
#define BUF_SIZ 2048
#define ETHERTYPE_CHAT 0x0701
#define BROADCAST_ADDR 0xffffffffffff

using namespace std;

void help(){
  cout << "Usage:" << endl;
  cout << " main -i <interface> [-h] [-u <username>]" << endl;
}

static void sigusr(int s){
  pthread_exit(NULL);
}

static void sigint(int s) {
  cout << endl;
  cout << "> " << flush;
}

struct msg_header {
  int payload_len;                //4
  char username[USERNAME_SIZ+1];  //11
};

#define ETHLEN sizeof(struct ether_header)
#define MSGLEN sizeof(struct msg_header)

void get_macaddr(int sockfd, uint8_t *src_mac, string interface){
  struct ifreq ifr;

  bzero(&ifr, sizeof(ifr));
  snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", interface.c_str());
  if(ioctl(sockfd, SIOCGIFHWADDR, &ifr) < 0){
    close(sockfd);
    perror("ioctl:SIOCGIFHWADDR");
    exit(EXIT_FAILURE);
  }
  memcpy(src_mac, ifr.ifr_hwaddr.sa_data, HWADDR_LEN*sizeof(uint8_t));
}

void set_promiscuous(int sockfd, string interface){
  struct ifreq ifr;

  bzero(&ifr, sizeof(ifr));
  snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", interface.c_str());
  if(ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0){
    perror("ioctl:SIOCGIFFLAGS");
    exit(EXIT_FAILURE);
  }
  ifr.ifr_flags |= IFF_PROMISC;
  if(ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0){
    perror("ioctl:SIOCSIFFLAGS");
    exit(EXIT_FAILURE);
  }
}

void bind_interface(int sockfd, string interface){
  if(setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE,
        interface.c_str(), interface.size()) == -1){
    close(sockfd);
    perror("SO_BINDTODEVICE");
    exit(EXIT_FAILURE);
  }
}

void recv_packet(int sockfd){
  char buf[BUF_SIZ];
  struct ether_header *eh = (struct ether_header *) buf;
  struct msg_header *mh = (struct msg_header*) (buf + ETHLEN);
  char *payload = buf + ETHLEN + MSGLEN;
  int ret;
  string output;

  signal(SIGUSR1, sigusr);

  while(1){
    bzero(buf, BUF_SIZ);
    ret = recvfrom(sockfd, buf, BUF_SIZ, 0, NULL, NULL);
    if(ret < sizeof(struct msg_header) + sizeof(struct ether_header)){
      cout << "sizeof(buf_recv): " << ret << endl;
      continue;
    }
    if(eh->ether_type != htons(ETHERTYPE_CHAT)){
      printf("ether_type: %04x\n", ntohs(eh->ether_type));
      continue;
    }
    cout << endl;
    cout << "[RECV] " << mh->username << ": ";
    output = "";
    for(int i=0; i<mh->payload_len; i++){
      output += payload[i];
    }
    cout << output << endl;
    cout << "> " << flush;
  }
}

void recv_input(int sockfd, string username, string interface, uint8_t *src_mac){
  uint8_t dst_mac[HWADDR_LEN];
  char *buf;
  int buflen = 0;
  struct ether_header *eh;
  struct msg_header *mh;
  char *payload;
  string cmd;
  struct sockaddr_ll device;
  int ret;

  memset (&device, 0, sizeof (device));
  if((device.sll_ifindex = if_nametoindex (interface.c_str())) == 0){
    perror("if_nametoindex");
    exit (EXIT_FAILURE);
  }

  while(1){
    cout << "> " << flush;
    if(!getline(cin, cmd)) break;
    if(cmd.size() == 0) continue;
    buflen = ETHLEN + MSGLEN + cmd.size();
    buf = (char *) malloc(buflen);
    eh = (struct ether_header *) buf;
    mh = (struct msg_header *) (buf + ETHLEN);
    payload = buf + ETHLEN + MSGLEN;
    assert(buf != NULL);
    assert(username.size() <= USERNAME_SIZ);
    eh->ether_type = htons(ETHERTYPE_CHAT);
    memset(eh->ether_dhost, 0xff, 6*sizeof(uint8_t));
    memcpy(eh->ether_shost, src_mac, 6*sizeof(uint8_t));
    snprintf(mh->username, USERNAME_SIZ + 1, "%s", username.c_str());
    mh->payload_len = cmd.size();
    memcpy(payload, cmd.c_str(), cmd.size());
    //cout << " username   : " << mh->username << endl;
    //cout << " payload_len: " << mh->payload_len << endl;
    //cout << " payload    : " << payload << endl;
    device.sll_family = AF_PACKET;
    device.sll_halen = ETH_ALEN;
    memset(device.sll_addr, 0xff, 6*sizeof(uint8_t));
    ret = sendto(sockfd, buf, buflen, 0,
        (struct sockaddr*)&device, sizeof(struct sockaddr_ll));
    if(ret < 0){
      perror("sendto");
    }
    free(buf);
    buf = NULL;
    cout << "[SEND] " << cmd << endl;
  }
}

void *tfn(void *arg){
  int *sockfd = (int *)arg;
  recv_packet(*sockfd);
}

int main(int argc, char *argv []){
  int opt;
  string username = "";
  string interface = "";
  int sockfd;
  uint8_t src_mac[HWADDR_LEN];
  pthread_t tid;
  int ret;

  while((opt = getopt(argc, argv, "i:u:h")) != -1){
    switch(opt){
      case 'i':
        interface = optarg;
        break;
      case 'u':
        username = optarg;
        break;
      default:
        help();
        exit(1);
    }
  }

  if(interface.size() == 0){
    help();
    exit(1);
  }

  if(username.size() == 0){
    srand(time(NULL));
    username = "u" + to_string(rand());
  }

  if(username.size() > USERNAME_SIZ){
    username.resize(USERNAME_SIZ);
  }

  cout << "username : " << username << endl;
  cout << "interface: " << interface << endl;

  if((sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETHERTYPE_CHAT))) < 0){
    perror("socket");
    exit(EXIT_FAILURE);
  }

  get_macaddr(sockfd, src_mac, interface);
  cout << "mac_addr : ";
  for(auto i=0; i<HWADDR_LEN; i++){
    printf("%02x", src_mac[i]);
    if(i != HWADDR_LEN - 1){
      cout << ":";
    }
    else{
      cout << endl;
    }
  }

  bind_interface(sockfd, interface);
  //set_promiscuous(sockfd, interface);

  ret = pthread_create(&tid, NULL, tfn, &sockfd);
  if(ret != 0){
    close(sockfd);
    perror("pthread_create");
    exit(EXIT_FAILURE);
  }

  signal(SIGINT, sigint);
  recv_input(sockfd, username, interface, src_mac);

  ret = pthread_kill(tid, SIGUSR1);
  if(ret != 0){
    close(sockfd);
    perror("pthread_kill");
    exit(EXIT_FAILURE);
  }

  ret = pthread_join(tid, NULL);
  if(ret != 0){
    close(sockfd);
    perror("pthread_join");
    exit(EXIT_FAILURE);
  }

  close(sockfd);
  return 0;
}
