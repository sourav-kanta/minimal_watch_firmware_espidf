#include "ble_types.h"
#include <ring_buffer.h>
#include <ble_consts.h>

static ble_msg_t tx_storage[BLE_FIFO_SIZE];
static ble_msg_t rx_storage[BLE_FIFO_SIZE];

static ring_buffer_t tx_fifo;
static ring_buffer_t rx_fifo;

bool ble_fifo_init(void)
{
    bool ok = true;

    ok &= ringbuf_init(&tx_fifo,
                       tx_storage,
                       BLE_FIFO_SIZE,
                       sizeof(ble_msg_t),
                       RB_OVERFLOW_DROP_OLDEST);

    ok &= ringbuf_init(&rx_fifo,
                       rx_storage,
                       BLE_FIFO_SIZE,
                       sizeof(ble_msg_t),
                       RB_OVERFLOW_DROP_OLDEST);

    return ok;
}

void ble_fifo_deinit(void)
{
    ringbuf_deinit(&tx_fifo);
    ringbuf_deinit(&rx_fifo);
}

static ring_buffer_t *get_fifo(ble_comm_type_t type)
{
    switch (type)
    {
        case BLE_TX:
            return &tx_fifo;

        case BLE_RX:
            return &rx_fifo;

        default:
            return NULL;
    }
}

bool add_ble_msg_to_queue(const ble_msg_t *msg, ble_comm_type_t type)
{
    if (msg == NULL)
    {
        return false;
    }

    if (msg->hdr.len > BLE_MAX_PAYLOAD_SIZE)
    {
        return false;
    }

    ring_buffer_t *fifo = get_fifo(type);

    if (fifo == NULL)
    {
        return false;
    }

    return ringbuf_push(fifo, msg);
}

bool get_next_ble_msg(ble_msg_t *msg, ble_comm_type_t type)
{
    if (msg == NULL)
    {
        return false;
    }

    ring_buffer_t *fifo = get_fifo(type);

    if (fifo == NULL)
    {
        return false;
    }

    return ringbuf_pop(fifo, msg);
}

