APPLICATION = ACM-ACN17-Demo

# If no BOARD is found in the environment, use this default:
BOARD ?= pba-d-01-kw2x

BOARD_WHITELIST := fox iotlab-m3 msba2 mulle native pba-d-01-kw2x samr21-xpro


# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../RIOT
#RIOTBASE ?= $(HOME)/RIOT

CFLAGS += -DDEVELHELP
CFLAGS += -DUSE_LINKLAYER
CFLAGS += -DCCNL_UAPI_H_
CFLAGS += -DUSE_SUITE_NDNTLV
CFLAGS += -DNEEDS_PREFIX_MATCHING
CFLAGS += -DNEEDS_PACKET_CRAFTING

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

USEMODULE += ps
USEMODULE += shell
USEMODULE += shell_commands
# Include packages that pull up and auto-init the link layer.
# NOTE: 6LoWPAN will be included if IEEE802.15.4 devices are present
USEMODULE += gnrc_netdev_default
USEMODULE += auto_init_gnrc_netif
USEMODULE += timex
USEMODULE += xtimer
USEMODULE += random
USEMODULE += prng_minstd
USEMODULE += l2filter_whitelist
#USEMODULE += netstats_l2

FEATURES_REQUIRED += periph_gpio
FEATURES_REQUIRED += periph_spi

USEPKG += tlsf

USEPKG += ccn-lite
USEMODULE += ccn-lite-utils

CFLAGS += -DCOMPAS_DEBUG=0
CFLAGS += -DCOMPAS_NAM_CACHE_LEN=8
CFLAGS += -DCCNL_CACHE_SIZE=10

ifneq (,$(NODE_A))
CFLAGS += -DNODE_A=1

USEMODULE += checksum
INCLUDES += -I$(CURDIR)

# for display
USEMODULE += pcd8544
TEST_PCD8544_SPI   ?= SPI_DEV\(0\)
TEST_PCD8544_CS    ?= GPIO_PIN\(2,4\)
# This differs from NODE_B because of a pin conflict with
# the MSA shield
TEST_PCD8544_RESET ?= GPIO_PIN\(0,19\)
TEST_PCD8544_MODE  ?= GPIO_PIN\(0,2\)

# export parameters
CFLAGS += -DTEST_PCD8544_SPI=$(TEST_PCD8544_SPI)
CFLAGS += -DTEST_PCD8544_CS=$(TEST_PCD8544_CS)
CFLAGS += -DTEST_PCD8544_RESET=$(TEST_PCD8544_RESET)
CFLAGS += -DTEST_PCD8544_MODE=$(TEST_PCD8544_MODE)
endif
ifneq (,$(NODE_B))
CFLAGS += -DNODE_B=1
# for light sensor to en/disable connectivity
USEMODULE += tcs37727
# for display
USEMODULE += pcd8544
TEST_PCD8544_SPI   ?= SPI_DEV\(0\)
TEST_PCD8544_CS    ?= GPIO_PIN\(2,4\)
TEST_PCD8544_RESET ?= GPIO_PIN\(0,1\)
TEST_PCD8544_MODE  ?= GPIO_PIN\(0,2\)

# export parameters
CFLAGS += -DTEST_PCD8544_SPI=$(TEST_PCD8544_SPI)
CFLAGS += -DTEST_PCD8544_CS=$(TEST_PCD8544_CS)
CFLAGS += -DTEST_PCD8544_RESET=$(TEST_PCD8544_RESET)
CFLAGS += -DTEST_PCD8544_MODE=$(TEST_PCD8544_MODE)
endif
ifneq (,$(NODE_C))
CFLAGS += -DNODE_C=1

# for LED matrix
USEPKG += u8g2
TEST_DISPLAY ?= u8g2_Setup_max7219_32x8_1
TEST_SPI ?= 0
TEST_PIN_CS ?= GPIO_PIN\(2,4\)
TEST_PIN_DC ?= GPIO_PIN\(0,2\)
TEST_PIN_RESET ?= GPIO_PIN\(3,1\)
CFLAGS += -DTEST_SPI=$(TEST_SPI)
CFLAGS += -DTEST_PIN_CS=$(TEST_PIN_CS)
CFLAGS += -DTEST_PIN_DC=$(TEST_PIN_DC)
CFLAGS += -DTEST_PIN_RESET=$(TEST_PIN_RESET)
CFLAGS += -DTEST_DISPLAY=$(TEST_DISPLAY)
endif

include $(RIOTBASE)/Makefile.include
