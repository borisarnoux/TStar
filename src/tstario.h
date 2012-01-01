#ifndef TSTARIO_H
#define TSTARIO_H

/* TStar IO aims at providing the user with fully asynchronous IO,
   that fits well with dataflow.
   It is following the philosophy of the incomplete linux-aio,
   but recreates a similar interface on top of epoll. */

// IO commands from libaio.h.


typedef enum io_iocb_cmd {
  // Those two are the only ones implemented.
  IO_CMD_PREAD = 0,
  IO_CMD_PWRITE = 1,

  IO_CMD_FSYNC = 2,
  IO_CMD_FDSYNC = 3,

  IO_CMD_POLL = 5,
  IO_CMD_NOOP = 6,
  IO_CMD_PREADV = 7,
  IO_CMD_PWRITEV = 8,
} io_iocb_cmd_t;



class TStarIO
{
public:
    TStarIO();
};

#endif // TSTARIO_H
