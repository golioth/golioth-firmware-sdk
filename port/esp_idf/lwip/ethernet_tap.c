#include <netif/ethernet.h>
#include "golioth_debug.h"

#define TAG "ethernet_tap"

extern err_t __real_ethernet_input(struct pbuf* p, struct netif* netif);

extern err_t __real_ethernet_output(
        struct netif* netif,
        struct pbuf* p,
        const struct eth_addr* src,
        const struct eth_addr* dst,
        u16_t eth_type);

err_t __wrap_ethernet_input(struct pbuf* p, struct netif* netif) {
    GLTH_LOGI(TAG, "wrapped ethernet_input");
    return __real_ethernet_input(p, netif);
}

err_t __wrap_ethernet_output(
        struct netif* netif,
        struct pbuf* p,
        const struct eth_addr* src,
        const struct eth_addr* dst,
        u16_t eth_type) {
    GLTH_LOGI(TAG, "wrapped ethernet_output");
    return __real_ethernet_output(netif, p, src, dst, eth_type);
}
