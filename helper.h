typedef struct {
    char ip_address[16];
    char port[6];
    char topic[50];
    char type;
    char content[1500];
} udp_info;

typedef struct {
    int sf;
    udp_info msg;
} channel;

typedef struct {
    int fd;
    char id[10];
    int aloc;
    channel *subscribed_topics;
    int no_topics;
    channel *unsent_topics;
    int no_unsent;
} client_info;