## HSI16 clock correction on CANable boards without a crystal: how it works and how much it helps

**Short answer:** at room temperature on the two chips tested, the correction brought **no measurable improvement**. The factory trim was already the best available code, with an error of about 0.01 ... 0.08 %. What it does provide is a **guaranteed bound**: whatever the oscillator does, the error ends up within about **+/-0.2 %** in a few seconds. Without the correction, the datasheet allows +/-1 % at 30 C and more over temperature. So it is a safety net, not an accuracy improvement, and it does not replace a crystal for demanding CAN FD.

## How it works

- **Why it is needed:** on boards without a crystal (`QUARTZ_FREQU = 0`, e.g. Multiboard, Openlightlabs), both FDCAN and the system clock come from the HSI16 RC oscillator through the PLL. The CRS only corrects HSI48 (the USB clock), which cannot feed the PLL.
- **Measurement:** the firmware counts HSI16-derived timer ticks over 1000 USB frames (1 s). The PC's crystal fixes the USB frame period, so the count gives the error to about 1 ppm. Delayed or lost SOF interrupts do not matter, because absolute timer values and USB frame numbers are used.
- **Correction:** the firmware adjusts HSITRIM in `RCC->ICSCR`. One step is 0.29 ... 0.34 % on the tested chip, so this is a coarse adjustment.
- **Uneven steps:** the HSITRIM scale is not smooth. Where HSICAL (factory value + HSITRIM - 64) steps onto a multiple of 64, the frequency drops by 4 ... 6 %. The positions are chip specific (the two chips tested had HSICAL 138 and 139 at HSITRIM 64), so the firmware computes them from HSICAL at runtime.
- **Safe behaviour:** while a CAN channel is open, only single steps are made and no jumps across those drops. While CAN is closed, the firmware can test the closest code on the other side of a drop for one window, and keeps it only if the measured error is smaller.

The code is in `Firmware/system.c` (`system_calibrate_hsi()`, called from the USB SOF interrupt).

## Measured results

The CANable's HSI16 was output on PA8 (`MCO_SOURCE` in `Firmware/settings.h`, HSI16 / 16 = 1 MHz) and measured with a NUCLEO-G491RE (crystal reference, TIM2 input capture, subproject `measure_freq_nuc_g491`). The CANable's own USB-based measurement agreed with it within **50 ... 70 ppm** in every test, up to +2.5 % error.

| Situation | Error |
|---|---|
| Factory trim (HSITRIM 64), correction off | -90 ... -790 ppm, drifting about -500 ppm in the first 10 s after reset while the chip warms up (second chip: +130 ... +410 ppm) |
| Detuned to +2.56 %, correction off | stays at +2.5 % |
| Correction on, starting from +2.56 / -2.7 / -1.0 / +1.9 / -3.3 % | back to the best code in **2.3 ... 3.6 s**, settled -750 ... +19 ppm |
| Simulation, 5000 random start codes, measured table of this chip | 100 % end at the best code |
| Simulation, 400 synthetic chips | 97 % within +/-1500 ppm, all within +/-2000 ppm |

## What limits it

- **Step size:** with steps of about 3000 ppm the error can only be guaranteed within about half a step + 200 ppm, i.e. +/-0.17 ... 0.2 %.
- **Oscillator wander:** the RC oscillator wanders +/-60 ... 100 ppm between 1 second windows. This cannot be corrected.
- **Little gain at room temperature:** the correction only acts when the error exceeds about 1700 ... 1900 ppm. The tested chips were 5 ... 10 times closer than that, so the correction stayed idle and the result equals the factory trim.

## When it helps

| Use | Without correction (datasheet) | With correction | Assessment |
|---|---|---|---|
| Classic CAN (<= 0.5 % recommended) | +/-1 % at 30 C, worse over temperature: marginal at worst case | +/-0.2 % | **Useful:** turns a marginal worst case into a comfortable margin |
| CAN FD, moderate data rates | marginal to failing at worst case | +/-0.2 % | **Helps**, but 0.2 % is still a noticeable part of the budget |
| CAN FD, high data rates (<= 0.3 % total recommended) | failing at worst case | +/-0.2 % | **Not enough** to be reliable, a crystal is the real fix |

The benefit grows with the deviation of the oscillator: at temperature extremes, with chips at the edge of the factory tolerance, or with ageing. In exactly those cases the measurement showed it reliably pulling errors of several percent back within about 3 s.

## Not tested

- Temperature: no heating or cooling tests, which is where the correction should matter most.
- Trim steps while CAN traffic is running.
- Long-term behaviour.
- More than two chips on real hardware. The rule for the drops is verified on one chip and computed for the second.

## Verdict

**Worth keeping as a safety net, not as an accuracy improvement.** It costs almost nothing (a 1 kHz interrupt and a decision once per second), it never made things worse in any test, and it bounds the clock error to about +/-0.2 % whatever the oscillator does. At room temperature on good chips it changes nothing. For demanding CAN FD it is not a substitute for a crystal.

---

# CANable-2.5-firmware-Slcan-and-Candlelight
Two new high quality, speed optimized firmwares for CANable adapters with lots of new features.

![CANable Adapter](https://github.com/user-attachments/assets/061f60ba-14a2-4896-866f-6226fc9123f6)


This is the first project that combines the two CANable firmware's Slcan and Candlelight into one code base.
Dozens of bugs have been fixed.
Dozens of new features have been added.
This is the first Candlelight firmware for the STM32Gxxx processor family that supports CAN FD and works without bugs.
However the new firmware is still 100% backward compatible with the legacy Slcan / Candlelight firmware.
The firmware has been tested on the STM32G431 on the isolated adapters from MKS Makerbase and Jhoinrch up to 10 Mbaud.
It works also on the STM32G473 dual CAN channel board from Oleksii.
The WeActStudio v1 firmware is compiled for the STM32G0B1 processor.
The firmware has been designed to be easily expandable for future processors and boards.
Precompiled binary firmware files can be uploaded to the CANable with the new Firmware Updater.
You can write your own CAN Application based on the ready-to-use classes in the C++, C# and Python demo projects for Windows or Linux.

<img width="577" height="484" alt="CANable STM32 Firmware Updater" src="https://github.com/user-attachments/assets/1364398f-fcd4-430e-aa8b-06cde32ce895" />


Please read the detailed 
- User Manual
- Slcan Developer Manual
- Candlelight Developer Manual
- Firmware Developer Manual

https://netcult.ch/elmue/CANable%20Firmware%20Update

Latest Updates:

https://netcult.ch/elmue/CANable%20Firmware%20Update#Source_Code

IMPORTANT:
If you get blocked by the firewall on netcult.ch, you can also read the "User & Developer Manual.htm" after downloading this Github repository.
