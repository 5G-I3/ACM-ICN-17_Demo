# Information-Centric Networking for the Industrial IoT
Code and documentation for our demo @ ACM-ICN-2017

## Documentation
- A two-page abstract describing the work:
[Abstract](https://inet.haw-hamburg.de/papers/gkslp-inii-17.pdf)
- A poster which has been presented during the demo session:
[Poster](ACM-ICN-17_Poster.pdf)

## Code

### CCN-lite
We made modifications to CCN-lite (v1) to include our Publish–Subscribe option.
The patch file that contains our modifications is provided in [ccn-lite.patch](ccn-lite.patch)
and needs to be applied onto [cn-uofbasel/ccn-lite@7b973a737dba47fe6c1ee2d58e06dd9a22209fde](https://github.com/cn-uofbasel/ccn-lite/commit/7b973a737dba47fe6c1ee2d58e06dd9a22209fde).

### RIOT
We made modifications to RIOT to include the modified version of CCN-lite.
The patch file is provided in [RIOT.patch](RIOT.patch) and needs to be applied onto
[RIOT-OS/RIOT@4a463c105db8b4a3b39cefc5784c25954fbac410](https://github.com/RIOT-OS/RIOT/commit/4a463c105db8b4a3b39cefc5784c25954fbac410).
Before applying the patch file, both sections (PKG\_URL and PKG\_VERSION) in
the file need to be adapted to point to the modified CCN-lite version (i.e. after applying [ccn-lite.patch](ccn-lite.patch)).

### Demo RIOT Application
The actual demo application is provided in [main.c](main.c) and [Makefile](Makefile).
The binary is flashed onto three [PhyNodes](https://github.com/RIOT-OS/RIOT/wiki/Board:-Phytec-phyWAVE-KW22).
The [Makefile](Makefile) needs to be adapted, so that the *RIOTBASE* variable points to
the patched RIOT version. In our demo, each of the three PhyNodes has a specific task
and the binary is built via *NODE_A=1 make ...*, *NODE_B=1 make ...* and *NODE_C=1 make ...*.

On the linux side, the patched CCN-Lite version is built with the *USE_WPAN=1* flag
and is started on a *wpan* interface with the *-w* flag.
In our demo, we used a RaspberryPi 3 with an [openlabs](http://openlabs.co/OSHW/Raspberry-Pi-802.15.4-radio) transceiver.
Our IEEE 802.15.4 setup is outlined in this [wiki](https://github.com/RIOT-Makers/wpan-raspbian/wiki/Create-a-generic-Raspbian-image-with-6LoWPAN-support).
