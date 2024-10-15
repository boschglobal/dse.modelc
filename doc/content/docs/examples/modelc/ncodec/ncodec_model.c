#include <dse/modelc/model.h>
#include <dse/ncodec/codec.h>

extern int put_rx_frame_to_queue(uint32_t, uint8_t*, size_t);
extern int get_tx_frame_from_queue(uint32_t*, uint8_t**, size_t*);

typedef struct {
    ModelDesc model;
    NCODEC*   nc;
} ExtendedModelDesc;

int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    ExtendedModelDesc* m = (ExtendedModelDesc*)model;
    NCodecCanMessage   msg = {};

    /* Message RX. */
    while (1) {
        if (ncodec_read(m->nc, &msg) < 0) break;
        put_rx_frame_to_queue(msg.frame_id, msg.buffer, msg.len);
    }

    /* Message TX. */
    ncodec_truncate(m->nc); /* Clear the codec buffer (i.e. Rx data). */
    while (get_tx_frame_from_queue(&msg.frame_id, &msg.buffer, &msg.len)) {
        ncodec_write(m->nc, &msg);
    }
    ncodec_flush(m->nc);

    /* Progress the Model time. */
    *model_time = stop_time;
    return 0;
}
