# GB Corp.

A Game Boy game first created for the [Game Boy Competition 2021](https://itch.io/jam/gbcompo21/). Then, it was latter enhanced, polished and extended to a full commercial release [available in physical format](https://yastuna-games.com/en/nintendo-game-boy/72-gb-corp.html) and as a [digital (ROM) version](https://drludos.itch.io/gb-corp).

by **Dr. Ludos** *(2021-2023)*

This is the source code, you can get a precompiled rom from here: \
https://drludos.itch.io/gb-corp

# How to build the source code

To compile the game, you'll need to use [GBDK-2020 v4.0.6](https://github.com/gbdk-2020/gbdk-2020/releases/tag/4.0.6). (it might need some tweaking / update to the [GBT Player music library](https://github.com/AntonioND/gbt-player) to compile on v4.1.0 and later).

Then, use the following commands:
*Use GBDK2020 to compile the main game program*
lcc -Wa-l -Wl-m -Wl-j -c -o gbcorp.o gbcorp.c

*Use GBDK2020 to compile the GBT Player output (C code from mod file)*
lcc -Wa-l -Wl-m -Wl-j -c -o music.o music.c

*Use GBDK2020 to assemble the GBT Player engine (ASM code)*
sdasgb -plosgff -I"libc" -c -o gbt_player.o gbt_player.s
sdasgb -plosgff -I"libc" -c -o gbt_player_bank1.o gbt_player_bank1.s

*And finally, assemble the ROM using GBDK2020 (set it as SGB compatible (-ys) and CGB compatible (-yc))*
lcc -Wl-yt0x1B -Wl-yo2 -Wl-ya1 -Wm-yn"GB-Corp" -Wm-ys -Wm-yc -o gbcorp.gb gbcorp.o music.o gbt_player.o gbt_player_bank1.o

The code is heavily commented to help you making your own GB games too, so feel free to contact me if you have any question about it. The 2P Player link mode was designed programmed by the talented [bbbbbr](https://github.com/bbbbbr/GBcorp).

***
Get all **my other games**: http://drludos.itch.io/ \
**Support my work** and get access to betas and prototypes:\
http://www.patreon.com/drludos
***

**GB Corp. is an incremental / idle game for the Game Boy, that rewards you for playing it on several console models**.

![gbcorp_gameplay](https://user-images.githubusercontent.com/42076899/135525540-b4eb3348-5e32-4480-a7b5-cb7b65ee458f.gif)

The core gameplay is quite simple. You hire consoles from the Game Boy line *(Original GB, GB Pocket, GB Color and Super Game Boy)*, and they work for your "push-button" company. When you have earned enough money, you can **hire more employees**, and **upgrade the ones you have** so they bring back more income. Also, you must be careful of their power levels: **change the batteries of your employees** when they are empty!

While the concept is quite simple, this game also has a unique feature: **the consoles you hire are effectively working only when you run the game on the same console model!** So, to put your GBC employees to work, you need to play the game on an actual Game Boy Color *(or set your emulator to "GBC mode")*. If you want to make your GBP employees active instead, you'll have to unplug the cartridge and put it into a Game Boy Pocket from your collection *(or change your emulator settings)*. The game uses a SRAM save, so the game progress is kept while switching consoles.

**This "console change" is a real gameplay mechanic** and not a simple gimmick. When you only have a handful of employees, changing batteries isn't a burden. But when you employ dozens of them, it costs loads of money. However, when you switch from one console model to another (say, you switch the cart from a GBC to an original GB), **all the active employees get a new set of batteries for free!**

Last but not least, the game also supports to **2 players mode over link cable**, so you can **link two different Game Boy console types** to  have two kind of employees working at the same time on screen, earning you more money! 

# How to play

The game controls are quite simple :

**D-Pad:** select employees

**A:** recharge batteries

**B:** upgrade employee / hire a new employee (if the cubicle is empty)

**SELECT:** fire an employee (warning, it costs money too!)

**START:** hold the button for 3 seconds to reset the SAVE (warning, all game progress will be lost forever!)

Each time you upgrade the level of an employee, or hire / fire someone, the cost of these actions increase. However, the more experienced your employees are, the more money they bring! For more precise numbers, each active employee will generate twice its recharge cost *(the $number in bracket after the current power level)* per second.

The game also feature a set of **10 goals (or achievements)** that you can try to reach if you are looking for some extra challenge.

# Do I really need several Game Boy to play this?

The best way to play this game is on a Flashcart with a collection of different Game Boy models at your disposal (at least 2, but if you only have 1 it'll work too, although you won't be able to benefit from the "free charge" bonus). The game is saving your progress automatically every second, so don't worry about losing any progress when switching the cartridge from one console to another.

But if you don't have a Game Boy at home, you can also enjoy the game ROM on an emulator. I heavily recommend you to use BGB: https://bgb.bircd.org/

In the emulator settings (Right Click > Options), on the "System" panel, you can configure which type of GB model must be active. Each time you modify the setting, you'll have to hit the "Apply" button, and then to reset the emulator (press * on your keyboard). That way, you can easily change from one console model to another.

Technically wise, the game relies on a MBC5 mapper for maximum compatibility with emulators and flashcarts. But as you can see from the small ROM size (32kb), it actually doesn't require a MBC but only 8kb of SRAM to run. So far, I have only been able to test the game with the original Everdrive-GB. But it should work on all flashcarts, including the ones that only feature 32kb of ROM + 8kb of SRAM.

If you use a GBA-only Flashcart, such as the EZ Flash Omega or the Everdrive GBA, the standard ROM of the game will crash on startup on those devices. Indeed, those Flashcart relies on a software emulator, namely Goomba Color (an excellent one), to play GB games on your GBA. Sadly Goomba Color doesn't support the link cable feature of GB Corp, and crashes. So, in the download section (over [itch.io](https://drludos.itch.io/gb-corp)), you'll also find a file titled "gbcorp_nolink.gb"- that is the same game with the link feature disabled. You can use this special version of the game to enjoy it on your GBA-only flashcart.

# Behind the scenes

This game is programmed 100% in C with the wonderful GBDK-2020: https://github.com/gbdk-2020/gbdk-2020

If you want to try your hand at making Game Boy games, I'm releasing this one as open-source with heavily commented source code. If you have any question about the source code, feel free to contact me!

This game was created from scratch for the [Game Boy Competition 2021](https://itch.io/jam/gbcompo21/rate/1212484): you'll find a lot of cool new GB games to play here!

At first, this game was created from scratch for the [Game Boy Competition 2021](https://itch.io/jam/gbcompo21/rate/1212484): you'll find a lot of cool new GB games to play here! Then, after the competition, I spent several months to polish and enhance the game, resulting in the final version that you can [buy on itch.io](https://drludos.itch.io/gb-corp). If you want to play the version of the game submitted to the competition, it's still available here as a freeware "prototype/demo" download *(original source code of the prototype is included too, it's the Release 1)*.

Enjoy!
