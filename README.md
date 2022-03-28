# Mousetrap

**Note** This project adds an esp32 to an electric mousetrap designed to kill mice. If this is something you find offense this project may not be for you.

## Background

I live in a fairly rural area and mice are a constant problem in our house. They get into the attic where they build homes in the insulation. I generally adhere to the "live and let live" philosophy however these mice are disruptive, destructive and filthy. They wake us up at night scurrying over the ceiling and their urination soaks into the drywall and stains the ceilings. 

Over the years I've looked at many methods to deal with mice in as humane a way as possible, mostly using traditional snap-trap style traps. In my experience they are simple and effective, most of the time they kill the mice instantly. There are a few problems however, knowing when a mouse has been caught as they only work one time without being reset, they are also very "intense" and the cleanup is not for the faint of heart. Lastly sometimes the mice are injured and not killed which makes me terribly sad at the thought of any creature suffering.

I've also looked at tipping bucket style traps which can either catch the mice live or drown them.  Drowning the mice seems rather cruel and I personally think killing the mice is more responsible than relocating them. When you relocate them they either become a pest to someone else or they will have no access to food/water and will likely die by starvation, the elements, or being caught by another animal. I understand if you have a different opinion here but my opinion is the most responsible way to manage mice problems is by killing them quickly and effectively. 

This is why I decided to invest in [electric mouse traps](https://www.victorpest.com/victor-multi-kill-electronic-mouse-trap-m260)

This particular model is able to work multiple times without interaction and looks to be as effective and humane as possible for killing mice.

## The project

Going into my attic is a chore and something I only want to do when I need to, so I was looking for a way to get some data out of these mousetraps to know when they needed attention.

So what I did was add an esp32 to detect when the trap is actuated as well as track the battery voltage.

The flavor of esp board I'm using comes from [dfrobot](https://wiki.dfrobot.com/FireBeetle_Board_ESP32_E_SKU_DFR0654#target_0), they are relatively inexpensive and have extremely low standby power in deep sleep mode.

### Wiring

#### Powering the esp32

The first challenge however is the max input voltage for these boards is 5.5V and the victor trap uses 4 1.5C batteries in series which makes over 6V with fully charged batteries.

To work around this I put a tap wire after 3 of the 4 batteries in series to give me a voltage in the range accepted by the esp32 board.

#### Detecting operation

The second challenge was getting an indicator of the trap being activated. Because of the voltage incompatability, I decided to try to keep my detection as separate as possible from the existing circuits. I discovered 2 limit switches used to control the rotation of the mechanism. These switches were also dual pole with only a single pole in use, I realized if I swapped the two wires of the top switch I could use the second unused pole to indicate when the top limit switch wasn't being depressed (which happens when the device actuates). This gave me a switch that I could use to pull a pullup to ground when the switch was relased and used to track the motion of the mechanism.


#### Detecting voltage

My solution here was to put a simple voltage divider between the output of all 4 batteries and one of the ADC input pins. I had some 60k resistor around so I made a simple /2 divider with 2 of them to measure the voltage. I don't love this solution however because it constantly leaks current and drains the batteries but it's only something like 50uA, good enough for now.

### Software

I threw together the fastest software I could to accomplish my goals of notifying me of the trap operation as well as sending a heartbeat with some stats so I could know the trap was still operating.

One of the more interesting challenges was when I discovered all my hard work to connect to the limit switch was kind of unnecessary as whenever the mouse trap activated the high voltage AC to the contact plates it was so noisy it would trigger the interrupt I was using on the switch to indicate the trap activation. At first I put some really basic delays in here but this wasn't ideal because when you power on the device it activates the high voltage AC briefly which triggered an event. I ended up putting a polling debounce on the switch to make sure it was held low for at least 1 sec before firing the event. It takes a couple seconds for the mechanism to move through a cycle and this seems to work well.

I'm using the [Loki Arduino](https://github.com/grafana/loki-arduino) library to send events and hearbeats to Loki.

To get this to build on platformio (the first time I've used it, and it was awesome!) I had to manually remove the WifiNINA library (which gets added as a dependency) and then in the `platformio.ini` file:

```
build_flags = 
	-D ARDUINO_DISABLE_ECCX08
```

There is some magic I put in place in the library to try to do this automatically which seems to work for Arduino's toolchain but not platformio, will need to revisit this later.

You'll also want to change out the certificate, this is the root cert for let's encrypt which is what I use to add TLS to my endpoints.

I also added an SHT31 temp and humidty sensor to one of my traps, I put the code for this in `#if` blocks so that it only compiles for one of the ID's





4.6V at the switch and it starts blinking


0.00036