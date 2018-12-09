int ptp_master(int port);
int ptp_slave(char* master_addr, int port);
void* master_thread(void* arg);