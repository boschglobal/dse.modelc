#include <dse/modelc/model.h>
#include <dse/ncodec/codec.h>

extern int put_rx_frame_to_queue(uint32_t, uint8_t*, size_t);
extern int get_tx_frame_from_queue(uint32_t*, uint8_t**, size_t*);

void do_bus_rx(SignalVector* sv, uint32_t idx)
{
    NCODEC* nc = sv->codec(sv, idx);

    while (1) {
        NCodecMessage msg = {};
        int           len = ncodec_read(nc, &msg);
        if (len < 0) break;
        put_rx_frame_to_queue(msg.frame_id, msg.buffer, msg.len);
    }
}

void do_bus_tx(SignalVector* sv, uint32_t idx)
{
    uint32_t id;
    uint8_t* msg;
    size_t   len;
    NCODEC*  nc = sv->codec(sv, idx);

    while (get_tx_frame_from_queue(&id, &msg, &len)) {
        ncodec_write(nc, &(struct NCodecMessage){
                             .frame_id = id,
                             .buffer = msg,
                             .len = len,
                         });
    }
    ncodec_flush(nc);
}
