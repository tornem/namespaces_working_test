
int create_netns_dir(void);
void ns_save(void);
void ns_restore(void);
int on_netns_del(const char *nsname);
void show_current_netns_id(void);