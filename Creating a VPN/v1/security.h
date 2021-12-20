#define MAP_SIZE 9973

unsigned int bbdecode(unsigned int x, unsigned int prikey);
unsigned int bbencode(unsigned int y, unsigned int pubkey);
unsigned long get_ip_int(char *ip_address);
int *get_acl();
char *get_my_ip();
