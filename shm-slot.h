#ifndef _SHM_SLOT_H_
#define _SHM_SLOT_H_

typedef struct slot {
    int tid;
    int static_req;
    int dyanmic_req;
    int total_req;
} slot_t;

#endif