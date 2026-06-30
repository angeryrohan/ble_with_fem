## **Adding nrf21540 RF FEM (GPIO Mode) support into any existing project** 

Documented on: **21/05/26** by _Rohan Singh_ 

_Final code (github):_ © _GitHub - angeryrohan/ble_with_fem: Taking 1M advertiser as the base, adding RF FEM Su…_ 

**CONTEXT: There are 3 modes (GPIO, SPI, GPIO+SPI). This document guides on implementing the GPIO mode.** 

## **Steps:** 

1. Edit build configuration 

> 2. Add CMake Argument: as **-DSHIELD=nrf21540ek** 

C main.c x 4 nrf21540ek.overlay 

$5 Edit Build Configuration (build) x 

) - 

~/mlwb/blinky_fem/sre/main.c 

**==> picture [289 x 345] intentionally omitted <==**

**----- Start of picture text -----**<br>
Edit Build Configuration (build)<br>@ nRF Connect SDK v3.2.4 v<br>Toolchain @)<br>&S nRF Connect SDK Toolchain v3.2.4 ad<br>Board target @ ®<br>nrf54115dk/nrf54115/cpuapp v<br>*) Nordic SoC Nordic Kits Custom All<br>Base configuration files (Kconfig fragments) @)<br>Select... ¥ Browse<br>Extra Kconfig fragments @<br>Select... - Browse<br>Base Devicetree overlays @<br>Select... ’ Browse<br>Extra Devicetree overlays @<br>Select... - ‘Browse<br>Scroll for more<br>**----- End of picture text -----**<br>


3. Click in your build file → Open the nrf21540ek.overlay by cliking here! 

4. Copy paste this code in nrf21540ek.overlay 

**CASE 1:** you have connected the ANT_SEL pin of the NRF21530 RF FEM to a GPIO of microcontroller. Example: 

## In that case, paste this: 

## **/ {** 

**nrf_radio_fem: name_of_fem_node {** 

**status="okay";** 

**compatible  = "nordic,nrf21540-fem";** 

**tx-en-gpios = <&gpio1 6 GPIO_ACTIVE_HIGH>;** 

**rx-en-gpios = <&gpio1 9 GPIO_ACTIVE_HIGH>;** 

**pdn-gpios   = <&gpio1 13 GPIO_ACTIVE_HIGH>;** 

**ant-sel-gpios = <&gpio1 10 GPIO_ACTIVE_HIGH>;** 

**};** 

**};** 

## **&radio {** 

**fem = <&nrf_radio_fem>;** 

**};** 

**CASE 2:** you have pulled UP or pulled DOWN the ANT_SEL gpio pin of the NRF 21540 RF FEM in the schematic. Paste this: 

## **/ {** 

**nrf_radio_fem: name_of_fem_node {** 

**status="okay";** 

**compatible  = "nordic,nrf21540-fem";** 

**tx-en-gpios = <&gpio1 6 GPIO_ACTIVE_HIGH>;** 

**rx-en-gpios = <&gpio1 9 GPIO_ACTIVE_HIGH>;** 

**pdn-gpios   = <&gpio1 13 GPIO_ACTIVE_HIGH>;** 

**};** 

**}** 

**};** 

## **&radio {** 

## **fem = <&nrf_radio_fem>;** 

## **};** 

4. Copy paste this code in prj.conf **(NOTE: CONFIG_BT_CTLR_TX_PWR_ANTENNA decides the TX Power, Input in dBM, max: 21dBM)** 

## **CONFIG_MPSL=y** 

## **CONFIG_MPSL_FEM=y** 

## **CONFIG_MPSL_FEM_NRF21540_GPIO=y** 

**CONFIG_MPSL_FEM_NRF21540_TX_GAIN_DB=20** 

**CONFIG_BT_CTLR_TX_PWR_ANTENNA=20** 

## 5. Perform a pristine build 

## **BEFORE:** 

**AFTER:** 

## **FAQ** 

## **GPIO Pin Selection** 

- The pins in the overlay (gpio1 6, gpio1 9, etc.) must match your physical wiring between the nRF54L15DK and the nRF21540 EK. These are not universal → verify against their schematic before copy-pasting 

- ant-sel-gpios is optional — only needed if you're using antenna diversity (THE RF FEM automatically switches between antennas) 

## **CONFIG values — when to change** 

- CONFIG_MPSL_FEM_NRF21540_TX_GAIN_DB=20 — 20dB is the nRF21540's fixed TX gain, don't change unless using a different FEM variant 

- CONFIG_BT_CTLR_TX_PWR_ANTENNA=21 — _**this is the main power controller**_ 

## **FAQ to add** 

- _"SPI mode vs GPIO mode — when do I care?"_ — GPIO mode is simpler but gives less runtime control; SPI mode lets you dynamically control gain. GPIO mode is fine for most BLE use cases 

