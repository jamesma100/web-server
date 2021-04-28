#ifndef _SHM_SLOT_H_
#define _SHM_SLOT_H_

typedef struct slot {
    long unsigned tid;
    int static_req;
    int dynamic_req;
    int total_req;
} slot_t;

#endif