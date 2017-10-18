/*
 * Copyright (C) 2017 Hamburg University of Applied Sciences
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       ACM ICN 17 Demo
 *
 * @author      Cenk Gündoğan <cenk.guendogan@haw-hamburg.de>
 * @author      Peter Kietzmann <peter.kietzmann@haw-hamburg.de>
 *
 * @}
 */

#include <stdio.h>
#include <string.h>

#include "xtimer.h"
#include "thread.h"
#include "shell.h"
#include "shell_commands.h"
#include "board.h"

#include "tlsf-malloc.h"
#include "ccnl-pkt-ndntlv.h"
#include "ccn-lite-riot.h"
#include "net/gnrc/netif.h"
#include "net/gnrc/netapi.h"
#include "net/gnrc.h"


/* MAC addresses of demo nodes for whitelisting */
/* mobile node */

 uint8_t demo_node_a_mac[]       ={ 0xd3, 0xc1, 0x6d, 0x73,
                                    0xab, 0x43, 0x13, 0x36 };
/* intermittent node */
uint8_t demo_node_b_mac[]       ={ 0xd3, 0xc1, 0x6d, 0x4d, \
                                   0xab, 0x0c, 0x13, 0x36 };
/* caching node */
uint8_t demo_node_c_mac[]       ={ 0x83, 0xd0, 0x6d, 0x5e, \
                                   0x52, 0xa5, 0x43, 0x2a };
/* RasPi */
uint8_t demo_node_cont_prox[]   ={ 0x18, 0xc0, 0xff, 0xee, \
                                   0x1a, 0xc0, 0xff, 0xee };


#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

#define EXPORTER_QUEUE_SIZE (8)
static msg_t _exporter_msg_queue[EXPORTER_QUEUE_SIZE];
#define EXPORTER_PERIODIC_INT   (5 * US_PER_SEC)

/* 10kB buffer for the heap should be enough for everyone */
#if NODE_A
#define TLSF_BUFFER     (10000 / sizeof(uint32_t))
#else
#define TLSF_BUFFER     (12288 / sizeof(uint32_t))
#endif

static uint32_t _tlsf_heap[TLSF_BUFFER];

kernel_pid_t ifs[GNRC_NETIF_NUMOF];

char stack[THREAD_STACKSIZE_MAIN];
char exporter_stack[THREAD_STACKSIZE_MAIN];

#define RF_CHANNEL_ALTER (11)
#define RF_CHANNEL       (17)

#if NODE_A || NODE_B
/* for the display */
#include "pcd8544.h"
static pcd8544_t display_dev;

#if NODE_A
#define PUB_CONTENT 0x6666


static unsigned char _out[CCNL_MAX_PACKET_SIZE];
#endif /* NODE_A */

#if NODE_B
#define LIGHT_RGB_THRESH    (500)
#include "tcs37727.h"
#include "tcs37727_params.h"
/* Sum of RGB values */
static bool node_status=true;
static tcs37727_t rgb_dev;
#endif/* NODE_B */
#endif /* NODE_A || NODE_B */

#if NODE_C
/* for LED matrix */
#define MAX_LED_ROWS        (32)
#include "u8g2.h"
static u8g2_t u8g2;

static int num_cache_entries = 0;

static gpio_t pins[] = {
    [U8X8_PIN_CS] = TEST_PIN_CS,
    [U8X8_PIN_DC] = TEST_PIN_DC,
    [U8X8_PIN_RESET] = TEST_PIN_RESET
};

static uint32_t pins_enabled = (
    (1 << U8X8_PIN_CS) +
    (1 << U8X8_PIN_DC) +
    (1 << U8X8_PIN_RESET)
);

static void _matrix_set_cache(int cache)
{
    if (cache * 2 < MAX_LED_ROWS){
        for (int i = 0; i < MAX_LED_ROWS; i++) {
            if (i < cache * 2) {
                u8g2_SetDrawColor(&u8g2, 1);
            }
            else {
                u8g2_SetDrawColor(&u8g2, 0);
            }
            u8g2_DrawVLine(&u8g2, i, 0, 8);
            u8g2_NextPage(&u8g2);
        }
    }
}
static void _matrix_inc_cache(void)
{
    if (num_cache_entries < MAX_LED_ROWS){
        u8g2_SetDrawColor(&u8g2, 1);
        u8g2_DrawVLine(&u8g2, num_cache_entries, 0, 8);
        u8g2_NextPage(&u8g2);
        num_cache_entries++;
    }
}
static void _matrix_dec_cache(void)
{
    if (num_cache_entries > -1){
        u8g2_SetDrawColor(&u8g2, 0);
        u8g2_DrawVLine(&u8g2, num_cache_entries, 0, 8);
        u8g2_NextPage(&u8g2);
        num_cache_entries--;
    }
}

static void _custom_ledmatrix_test(void){
    for (int i=0;i < MAX_LED_ROWS; i++){
        _matrix_inc_cache();
        xtimer_usleep(5000);
    }
    for (int i=0;i < MAX_LED_ROWS+1; i++){
        _matrix_dec_cache();
        xtimer_usleep(5000);
    }
}
#endif/* NODE_C */

kernel_pid_t exporter_pid;

#define MAX_ADDR_LEN            (8U)
uint8_t hwaddr[MAX_ADDR_LEN];
char hwaddr_str[MAX_ADDR_LEN * 3];

#ifndef NODE_LABEL
#define NODE_LABEL "forwarder"
#endif

#ifdef NODE_A
#define NODE_ID "Node A"
#endif
#ifdef NODE_B
#define NODE_ID "Node B"
#endif
#ifdef NODE_C
#define NODE_ID "Node C"
#endif

// static const char *node_label = NODE_LABEL;

#if NODE_A
static bool _pub(struct ccnl_relay_s *ccnl)
{
    char name[32], name2[32];
    char sensor_reading[64];
    int suite = CCNL_SUITE_NDNTLV;
    int offs = CCNL_MAX_PACKET_SIZE;
    char* use_prefix;

    int reading=(int)xtimer_now_usec();

    if(ccnl->dodag.rank == COMPAS_DODAG_UNDEF &&
       !compas_dodag_floating(ccnl->dodag.flags)){
        return false;
    }

    use_prefix = ccnl->dodag.prefix.prefix;

    int name_len = sprintf(name, "%s/%s/gas/%u", use_prefix, "1", (unsigned)xtimer_now_usec());
    name[name_len]='\0';
    memcpy(name2, name, name_len);
    name2[name_len]='\0';

    int sensor_reading_len = sprintf(sensor_reading, "%i", reading);
    sensor_reading[sensor_reading_len]='\0';

    struct ccnl_prefix_s *prefix = ccnl_URItoPrefix(name, suite, NULL, NULL);
    int arg_len = ccnl_ndntlv_prependContent(prefix, (unsigned char*) sensor_reading,
        sensor_reading_len, NULL, NULL, &offs, _out);
    free_prefix(prefix);

    unsigned char *olddata;
    unsigned char *data = olddata = _out + offs;

    int len;
    unsigned typ;

    if (ccnl_ndntlv_dehead(&data, &arg_len, (int*) &typ, &len) || typ != NDN_TLV_Data) {
        puts("ERROR in _pub");
        return false;
    }

    struct ccnl_content_s *c = 0;
    struct ccnl_pkt_s *pk = ccnl_ndntlv_bytes2pkt(typ, olddata, &data, &arg_len);
    c = ccnl_content_new(ccnl, &pk);
    ccnl_content_add2cache(ccnl, c);
    c->flags |= CCNL_CONTENT_FLAGS_STALE;

    compas_name_t cname;
    compas_name_init(&cname, name2, strlen(name2));

    compas_nam_cache_add(&ccnl->dodag, &cname, NULL);
#if defined(CCNL_RIOT)
    if (exporter_pid != KERNEL_PID_UNDEF) {
        msg_t m = { .type = EXPORTER_EVENT_NAM_CACHE_ADD };
        msg_try_send(&m, exporter_pid);
    }
#endif
    msg_t m = { .type = COMPAS_PUB_MSG };
    msg_try_send(&m, ccnl->pid);

    return true;
}
#endif /* NODE_A */

/* This thread is used for periodic publishing on NODE A
or detecting darkness wich triggers partinioning on NODE B */
void *publisher(void *arg)
{
    while (1) {
#if NODE_A
        struct ccnl_relay_s *ccnl = (struct ccnl_relay_s *) arg;
        _pub(ccnl);
#endif
#if NODE_B
    tcs37727_data_t data;
    tcs37727_read(&rgb_dev, &data);
    uint32_t sum_rgb = data.red + data.green + data.blue;
    printf("RGB light value is %" PRIu32 " threshold is: %i\n", sum_rgb, LIGHT_RGB_THRESH);
    if ( node_status && sum_rgb < LIGHT_RGB_THRESH){
        /* make node unavailable */
        uint16_t chan = RF_CHANNEL_ALTER;
        printf("Set RF channel to %i\n", chan);
        gnrc_netapi_set(ifs[0], NETOPT_CHANNEL, 0, &chan, sizeof(chan));
        char chan_str[5] = { 0, 0, 0, 0, 0 };
        sprintf(chan_str, "C:%2d", chan);
        pcd8544_write_s(&display_dev, 10, 2, chan_str);
        node_status=false;
    }
    else if(!node_status && sum_rgb > LIGHT_RGB_THRESH){
        /* make node available again */
        uint16_t chan = RF_CHANNEL;
        printf("Set RF channel to %i\n", chan);
        gnrc_netapi_set(ifs[0], NETOPT_CHANNEL, 0, &chan, sizeof(chan));
        char chan_str[5] = { 0, 0, 0, 0, 0 };
        sprintf(chan_str, "C:%2d", chan);
        pcd8544_write_s(&display_dev, 10, 2, chan_str);
        node_status=true;
    }
#endif
        xtimer_sleep(5);
    }
    return NULL;
}

static bool _send(struct ccnl_relay_s *ccnl, gnrc_pktsnip_t *pkt, uint8_t *addr, uint8_t addr_len)
{
    gnrc_pktsnip_t *hdr = gnrc_netif_hdr_build(NULL, 0, addr, addr_len);

    if (hdr == NULL) {
        puts("error: packet buffer full");
        gnrc_pktbuf_release(pkt);
        return false;
    }

    LL_PREPEND(pkt, hdr);

    if (!addr) {
        gnrc_netif_hdr_t *nethdr = (gnrc_netif_hdr_t *)hdr->data;
        nethdr->flags = GNRC_NETIF_HDR_FLAGS_BROADCAST;
    }

    struct ccnl_if_s *ifc = NULL;
    for (int i = 0; i < ccnl->ifcount; i++) {
        if (ccnl->ifs[i].if_pid != 0) {
            ifc = &ccnl->ifs[i];
            break;
        }
    }

    if (gnrc_netapi_send(ifc->if_pid, pkt) < 1) {
        puts("error: unable to send\n");
        gnrc_pktbuf_release(pkt);
        return false;
    }
    return true;
}

void *meta_exporter(void *arg)
{
    struct ccnl_relay_s *ccnl = (struct ccnl_relay_s *) arg;
    uint8_t buffer[128];
    compas_dodag_t *dodag = &ccnl->dodag;
    (void) dodag;

    msg_init_queue(_exporter_msg_queue, EXPORTER_QUEUE_SIZE);
    msg_t m;

    xtimer_t periodic_timer;
    msg_t periodic_msg = { .type = EXPORTER_PERIODIC };

    xtimer_set_msg(&periodic_timer, EXPORTER_PERIODIC_INT, &periodic_msg, thread_getpid());

    while(1) {
        msg_receive(&m);
#if (NODE_A)
        if(m.type == PUB_CONTENT){
            _pub(ccnl);
            continue;
        }
#endif

        if (m.type == EXPORTER_EVENT_PARENT_REFRESH) {
            /* PARENT REFRESH */
#if (NODE_A) || (NODE_B)
            char rank_str[5] = { 0, 0, 0, 0, 0 };
            sprintf(rank_str, "R:%2d", ccnl_relay.dodag.rank);
            pcd8544_write_s(&display_dev, 6, 0, "  ");
            pcd8544_write_s(&display_dev, 10, 0, rank_str);
#endif
            continue;
        }
        else if (m.type == EXPORTER_EVENT_CON_CACHE_ADD) {
            continue;
        }

        size_t len = 0;
        uint8_t tmp = 0;

        buffer[len++] = 0x00;

        /* ID / HWADDR */
        tmp = (uint8_t) strlen(NODE_ID);
        buffer[len++] = 0x00;
        buffer[len++] = tmp;
        memcpy((buffer + len), NODE_ID, tmp);
        len += tmp;

        if (m.type == EXPORTER_EVENT_PARENT_ADD  || m.type ==  EXPORTER_PERIODIC || m.type == EXPORTER_EVENT_PARENT_REFRESH) {
            /* PARENT ADD */
            if(ccnl->dodag.rank != COMPAS_DODAG_UNDEF &&
               !compas_dodag_floating(ccnl->dodag.flags)){
                buffer[len++] = 0x04;
                buffer[len++] = ccnl_relay.dodag.parent.face.face_addr_len;
                memcpy((buffer + len), ccnl_relay.dodag.parent.face.face_addr, ccnl_relay.dodag.parent.face.face_addr_len);
                len += ccnl_relay.dodag.parent.face.face_addr_len;
#if (NODE_A) || (NODE_B)
                char rank_str[5] = { 0, 0, 0, 0, 0 };
                sprintf(rank_str, "R:%2d", ccnl_relay.dodag.rank);
                pcd8544_write_s(&display_dev, 6, 0, "  ");
                pcd8544_write_s(&display_dev, 10, 0, rank_str);
#endif
            }

            /* CURR CACHE */
            buffer[len++] = 0x02;
            buffer[len++] = 1;
            tmp = 0;
            unsigned i;
            for (i = 0; i < COMPAS_NAM_CACHE_LEN; ++i) {
                if (ccnl_relay.dodag.nam_cache[i].in_use) {
                    tmp++;
                }
            }
            buffer[len++] = tmp;

            /* MAX CACHE */
            buffer[len++] = 0x03;
            buffer[len++] = 1;
            buffer[len++] = COMPAS_NAM_CACHE_LEN;
        }
        else if (m.type == EXPORTER_EVENT_PARENT_DROP) {
            /* PARENT DROP */
            buffer[len++] = 0x05;
            buffer[len++] = ccnl_relay.dodag.parent.face.face_addr_len;
            memcpy((buffer + len), ccnl_relay.dodag.parent.face.face_addr, ccnl_relay.dodag.parent.face.face_addr_len);
            len += ccnl_relay.dodag.parent.face.face_addr_len;
#if (NODE_A) || (NODE_B)
            char rank_str[5] = { 0, 0, 0, 0, 0 };
            sprintf(rank_str, "R:%2d", ccnl_relay.dodag.rank);
            pcd8544_write_s(&display_dev, 6, 0, "TO");
            pcd8544_write_s(&display_dev, 10, 0, rank_str);
#endif
        }
        else if (m.type == EXPORTER_EVENT_NAM_CACHE_ADD) {
            /* CURR CACHE */
            buffer[len++] = 0x02;
            buffer[len++] = 1;
            tmp = 0;
            unsigned i;
            for (i = 0; i < COMPAS_NAM_CACHE_LEN; ++i) {
                if (ccnl_relay.dodag.nam_cache[i].in_use) {
                    tmp++;
                }
            }
            buffer[len++] = tmp;

            /* MAX CACHE */
            buffer[len++] = 0x03;
            buffer[len++] = 1;
            buffer[len++] = COMPAS_NAM_CACHE_LEN;
#if NODE_C
            _matrix_set_cache(tmp);
#endif
#if (NODE_A || NODE_B)
            char nam_str[5] = { 0, 0, 0, 0, 0 };
            sprintf(nam_str, "N:%2d", tmp);
            pcd8544_write_s(&display_dev, 10, 4, nam_str);
#endif
            printf("ADD TO CACHE; CUR CACHE: %d\n", tmp);
        }
        else if (m.type == EXPORTER_EVENT_NAM_CACHE_DEL) {
            /* CURR CACHE */
            buffer[len++] = 0x02;
            buffer[len++] = 1;
            tmp = 0;
            unsigned i;
            for (i = 0; i < COMPAS_NAM_CACHE_LEN; ++i) {
                if (ccnl_relay.dodag.nam_cache[i].in_use) {
                    tmp++;
                }
            }
            buffer[len++] = tmp;

            /* MAX CACHE */
            buffer[len++] = 0x03;
            buffer[len++] = 1;
            buffer[len++] = COMPAS_NAM_CACHE_LEN;
#if NODE_C
            _matrix_set_cache(tmp);
#endif
#if (NODE_A || NODE_B)
            char nam_str[5] = { 0, 0, 0, 0, 0 };
            sprintf(nam_str, "N:%2d", tmp);
            pcd8544_write_s(&display_dev, 10, 4, nam_str);
#endif
        }

        gnrc_pktsnip_t *pkt = gnrc_pktbuf_add(NULL, buffer, len, GNRC_NETTYPE_CCN);

        if (pkt == NULL) {
            puts("error: packet buffer full");
            continue;
        }

        _send(ccnl, pkt, NULL, 0);
        xtimer_set_msg(&periodic_timer, EXPORTER_PERIODIC_INT, &periodic_msg, thread_getpid());
    }
    return NULL;
}

#if (NODE_A)
static void _btn_cb_node_a(void *arg)
{
    (void)arg;
    msg_t _btn_handle = {.type = PUB_CONTENT, .content.ptr = NULL};
    msg_try_send(&_btn_handle, exporter_pid);
}
#endif

int main(void)
{
    (void) puts("Welcome to RIOT!");

    tlsf_create_with_pool(_tlsf_heap, sizeof(_tlsf_heap));
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);

    ccnl_core_init();

    ccnl_start();

    /* set the relay's PID, configure the interface to use CCN nettype */
    if ((gnrc_netif_get(ifs) == 0) || (ccnl_open_netif(ifs[0], GNRC_NETTYPE_CCN) < 0)) {
        puts("Error registering at network interface!");
        return -1;
    }

    uint16_t src_len = 8;
    gnrc_netapi_set(ifs[0], NETOPT_SRC_LEN, 0, (uint16_t *)&src_len, sizeof(uint16_t));

    int res = gnrc_netapi_get(ifs[0], NETOPT_ADDRESS, 0, hwaddr, sizeof(hwaddr));
    gnrc_netif_addr_to_str(hwaddr_str, sizeof(hwaddr_str), hwaddr, res);

    uint16_t chan = RF_CHANNEL;
    uint16_t tx_power = 1024;
    netopt_enable_t cca = NETOPT_DISABLE;
    gnrc_netapi_set(ifs[0], NETOPT_CHANNEL, 0, &chan, sizeof(chan));
    gnrc_netapi_set(ifs[0], NETOPT_TX_POWER, 0, &tx_power, sizeof(tx_power));
    gnrc_netapi_set(ifs[0], NETOPT_AUTOCCA, 0, &cca, sizeof(netopt_enable_t));

    /* Initialize peripherals and blacklisting */
#if (NODE_A) || (NODE_B)
    /* initialize display */
    if (pcd8544_init(&display_dev, TEST_PCD8544_SPI, TEST_PCD8544_CS,
                     TEST_PCD8544_RESET, TEST_PCD8544_MODE) != 0) {
        puts("Failed to initialize PCD8544 display");
        return 1;
    }
    pcd8544_poweron(&display_dev);
    pcd8544_set_contrast(&display_dev, 66); /* determined by looking at it */
    pcd8544_riot(&display_dev);
    xtimer_sleep(2);
    pcd8544_clear(&display_dev);
    pcd8544_write_s(&display_dev, 0, 2, "ACMICN'17");
    pcd8544_write_s(&display_dev, 10, 0, "R:XX");
#if NODE_A
    puts("Initialize Node A");
    pcd8544_write_s(&display_dev, 0, 0, "N:A");

    gnrc_netapi_set(ifs[0], NETOPT_L2FILTER, 0, demo_node_cont_prox, 8);
    gnrc_netapi_set(ifs[0], NETOPT_L2FILTER, 0, demo_node_c_mac, 8);

    gpio_init_int(BTN0_PIN, GPIO_IN_PU, GPIO_FALLING, _btn_cb_node_a, NULL);

#elif NODE_B
    puts("Initialize Node B");
    pcd8544_write_s(&display_dev, 0, 0, "N:B");

    /* Initialize light sensor which en/disabels conenctivity */
    if (tcs37727_init(&rgb_dev, &tcs37727_params[0]) != TCS37727_OK) {
        puts("Error initializing light sensor");
    }
    /* Set the nodes initial state (should be "online" usually) */
    tcs37727_data_t data;
    tcs37727_read(&rgb_dev, &data);
    if ( (data.red + data.green + data.blue) < LIGHT_RGB_THRESH ){
        chan = RF_CHANNEL_ALTER;
        printf("Set RF channel to %i\n", chan);
        gnrc_netapi_set(ifs[0], NETOPT_CHANNEL, 0, &chan, sizeof(chan));
        node_status = false;
    }
    else{
        chan = RF_CHANNEL;
        printf("Set RF channel to %i\n", chan);
        gnrc_netapi_set(ifs[0], NETOPT_CHANNEL, 0, &chan, sizeof(chan));
        node_status = true;
    }

    gnrc_netapi_set(ifs[0], NETOPT_L2FILTER, 0, demo_node_cont_prox, 8);
    gnrc_netapi_set(ifs[0], NETOPT_L2FILTER, 0, demo_node_c_mac, 8);
    //gnrc_netapi_set(ifs[0], NETOPT_L2FILTER, 0, demo_node_a_mac, 8);
#endif
#endif /* NODE_A || NODE_B */
#if NODE_C
    puts("Initialize Node C");

    TEST_DISPLAY(&u8g2, U8G2_R0, u8x8_byte_riotos_hw_spi, u8x8_gpio_and_delay_riotos);
    /* Initialize LED matrix */
    u8g2_SetPins(&u8g2, pins, pins_enabled);
    u8g2_SetDevice(&u8g2, TEST_SPI);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    _custom_ledmatrix_test();

    gnrc_netapi_set(ifs[0], NETOPT_L2FILTER, 0, demo_node_a_mac, 8);
    gnrc_netapi_set(ifs[0], NETOPT_L2FILTER, 0, demo_node_b_mac, 8);
#endif
#if NODE_A || NODE_B
    char chan_str[5] = { 0, 0, 0, 0, 0 };
    sprintf(chan_str, "C:%2d", chan);
    pcd8544_write_s(&display_dev, 10, 2, chan_str);
#endif


#ifndef COMPAS_ROOT
#if NODE_B
    thread_create(stack, sizeof(stack), THREAD_PRIORITY_MAIN - 1,
                  THREAD_CREATE_STACKTEST, publisher, &ccnl_relay, "pub");
#endif
#endif
    exporter_pid = thread_create(exporter_stack, sizeof(exporter_stack), THREAD_PRIORITY_MAIN - 2,
                  THREAD_CREATE_STACKTEST, meta_exporter, &ccnl_relay, "meta-exporter");
#ifdef COMPAS_ROOT
    compas_dodag_init_root(&ccnl_relay.dodag, "/HAW", strlen("/HAW"));
    trickle_init(&ccnl_relay.pam_trickle, TRICKLE_IMIN, TRICKLE_IMAX, TRICKLE_REDCONST);
    uint64_t trickle_int = trickle_next(&ccnl_relay.pam_trickle);
    xtimer_set_msg64(&ccnl_relay.compas_pam_timer, trickle_int * 1000,
                     &ccnl_relay.compas_pam_msg, ccnl_relay.pid);

#endif

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
