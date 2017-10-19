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
and needs to be applied onto cn-uofbasel/ccn-lite@7b973a737dba47fe6c1ee2d58e06dd9a22209fde.

### RIOT
We made modifications to RIOT to include the modified version of CCN-lite.
The patch file is provided in [RIOT.patch](RIOT.patch). Before applying the
patch file, both sections (PKG\_URL and PKG\_VERSION) in the file need to be adapted
to point to the modified CCN-lite version (i.e. after applying [ccn-lite.patch](ccn-lite.patch).
