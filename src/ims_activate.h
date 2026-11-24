#ifndef IMS_ACTIVATE_H
#define IMS_ACTIVATE_H

int ims_activate_begin(void);
int ims_activate_poll(int queue);
void ims_activate_end(int queue);

#endif
