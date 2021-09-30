# GB Corp.

A Game Boy game for the [Game Boy Competition 2021](https://itch.io/jam/gbcompo21/)

by **Dr. Ludos** *(2021)*

This is the source code, you can get a precompiled rom from here: \
https://drludos.itch.io/gb-corp

 
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

# How to play

The game controls are quite simple :

**D-Pad:** select employees

**A:** recharge batteries

**B:** upgrade employee / hire a new employee (if the cubicle is empty)

**SELECT:** fire an employee (warning, it costs money too!)

**START:** hold the button for 3 seconds to reset the SAVE (warning, all game progress will be lost forever!)

Each time you upgrade the level of an employee, or hire / fire someone, the cost of these actions increase. However, the more experienced your employees are, the more money they bring! For more precise numbers, each active employee will generate twice its recharge cost *(the $number in bracket after the current power level)* per second.

# Do I really need several Game Boy to play this?

The best way to play this game is on a Flashcart with a collection of different Game Boy models at your disposal (at least 2, but if you only have 1 it'll work too, although you won't be able to benefit from the "free charge" bonus). The game is saving your progress automatically every second, so don't worry about losing any progress when switching the cartridge from one console to another.

But if you don't have a Game Boy at home, you can also enjoy the game ROM on an emulator. I heavily recommend you to use BGB: https://bgb.bircd.org/

In the emulator settings (Right Click > Options), on the "System" panel, you can configure which type of GB model must be active. Each time you modify the setting, you'll have to hit the "Apply" button, and then to reset the emulator (press * on your keyboard). That way, you can easily change from one console model to another.

Technically wise, the game relies on a MBC5 mapper for maximum compatibility with emulators and flashcarts. But as you can see from the small ROM size (32kb), it actually doesn't require a MBC but only 8kb of SRAM to run. So far, I have only been able to test the game with the original Everdrive-GB. But it should work on all flashcarts, including the ones that only feature 32kb of ROM + 8kb of SRAM.

# Behind the scenes

This game is programmed 100% in C with the wonderful GBDK-2020: https://github.com/gbdk-2020/gbdk-2020

If you want to try your hand at making Game Boy games, I'm releasing this one as open-source with heavily commented source code. If you have any question about the source code, feel free to contact me!

This game was created from scratch for the [Game Boy Competition 2021](https://itch.io/jam/gbcompo21/rate/1212484): you'll find a lot of cool new GB games to play here!

Enjoy!

