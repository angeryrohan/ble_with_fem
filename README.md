## Adding nrf21540 RF FEM (GPIO Mode) support into any existing project
Documented on: 21/05/26 by Rohan Singh.





## Steps:

### 1. Edit build configuration
<img width="704" height="489" alt="Screenshot 2026-06-30 at 1 45 08 PM" src="https://github.com/user-attachments/assets/971689a8-cba6-45b2-bf6f-455c64a32fa8" />

### 2. Add CMake Argument:-DSHIELD=nrf21540 ek
<img width="703" height="522" alt="Screenshot 2026-06-30 at 1 49 03 PM" src="https://github.com/user-attachments/assets/4d5328af-d36a-40ef-8ea9-a976124d5bd7" />

### 3. Click in your buildfile → Open the nrf21540ek.overlay by cliking here!
<img width="498" height="120" alt="Screenshot 2026-06-30 at 1 50 23 PM" src="https://github.com/user-attachments/assets/813ebbae-08a3-4717-a6ed-dfd0dc2b51fa" />

### 4. Copy paste this code in nrf21540ek.overlay

CASE 1: You have connected the ANT_SEL pin of the NRF21530 RF FEM to a GPIO of microcontroller. Example:
<img width="685" height="349" alt="Screenshot 2026-06-30 at 1 50 48 PM" src="https://github.com/user-attachments/assets/b735d8ca-f815-4149-bd0a-b9864ba3bc16" />


In that case, paste this: 
`
/ {nrf_radio_fem: name_of_fem_node {status="okay"; compatible = "nordic, nrf21540-fem"; tx-en-gpios = <&gpio 1 6 GPIO_ACTIVE_HIGH>; rx-en-gpios = <&gpio 1 9 GPIO_ACTIVE_HIGH>; pdn-gpios = <&gpio 1 13 GPIO_ACTIVE_HIGH>; ant-sel-gpios = <&gpio 1 10 GPIO_ACTIVE_HIGH>;};}; &radio {fem = <&nrf_radio_fem>;};
`

CASE 2: You have pulled UP or pulled DOWN the ANT_SEL gpio pin of the NRF 21540 RF FEM
in the schematic. Paste this:

`
/ {
nrf_radio_fem: name_of_fem_node {
status="okay";
compatible = "nordic, nrf21540-fem";
tx-en-gpios = <&gpio 1 6 GPIO_ACTIVE_HIGH>;
rx-en-gpios = <&gpio 1 9 GPIO_ACTIVE_HIGH>;
pdn-gpios = <&gpio 1 13 GPIO_ACTIVE_HIGH>;
};
}
};
&radio {
fem = <&nrf_radio_fem>;
};
`


### 4. Copy paste this code in prj.conf (NOTE: CONFIG_BT_CTLR_TX_PWR_ANTENNA decides the TX Power,

`
CONFIG_MPSL=y CONFIG_MPSL_FEM=y CONFIG_MPSL_FEM_NRF21540_GPIO=y CONFIG_MPSL_FEM_NRF21540_TX_GAIN_DB=20 CONFIG_BT_CTLR_TX_PWR_ANTENNA=20
`


### 5. Perform a pristine build
<img width="367" height="231" alt="Screenshot 2026-06-30 at 1 55 27 PM" src="https://github.com/user-attachments/assets/471b65c3-ae0f-4288-8a94-0c8668c664b3" />


## BEFORE: 
<img width="705" height="225" alt="Screenshot 2026-06-30 at 1 55 57 PM" src="https://github.com/user-attachments/assets/1653f8a2-1cdc-4b43-942e-eadcb6e4f8c4" />

## AFTER:
<img width="703" height="232" alt="Screenshot 2026-06-30 at 1 56 10 PM" src="https://github.com/user-attachments/assets/18c00b6d-844d-4aad-96cd-0da18927fb9b" />


<br>
<br>

## *FAQ*
### GPIO Pin Selection

- The pins in the overlay (gpio1 6, gpio1 9, etc.) must match your physical wiring between the nRF54L15DK and the
nRF21540 EK. These are not universal → verify against their schematic before copy-pasting
- ant-sel-gpios is optional — only needed if you're using antenna diversity (THE RF FEM automatically switches between
antennas)

### CONFIG values — when to change

- CONFIG_MPSL_FEM_NRF21540_TX_GAIN_DB=20  -> 20dB is the nRF21540's fixed TX gain, don't change unless using a different FEM variant
- CONFIG_BT_CTLR_TX_PWR_ANTENNA=21 — this is the main power controller
