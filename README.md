# Splutter

Spiraling, chaotic pitch shifting delays. Spooky, bubbling diminished chords. Laser-gun sounds. Ominous growling. Sparkling overtones. Another plug-in that does that? Yawn.

![Screenshot of splutter gui](./images/screenshot.png)

## Implementation details

This effect uses a variable delay length, moving in a sawtooth pattern to create a jittery, pitch-shifted delay. The discontinuity at the end of the sawtooth wave is softened by fading from the top of the sawtooth to the bottom over a fixed number of samples. 

Since we need to be able to jump from a short delay to a long delay very quickly, I use a constant-length delay line, with moving read and write pointers. The write pointer moves forward at a constant speed, while the read pointer jumps along the sawtooth pattern.

![Diagram of Splutter effect signal flow](./images/diagram.jpg)

## Future improvements

Some considerations for the future:

Make a knob for size of smoothing window-- at long grain sizes, the current setting (a fixed value of 1000 samples) is pretty jerky.

It would be nice to be able to sync the grain size and delay time with the DAW bpm.

Put EQ at the end? Putting it at the start can give some unexpected results-- it can make it feel like the EQ isn't "working" right away when there's a long delay.

Certain EQ settings can create feedback loops at high feedback levels. Fix this (maybe with a lower Q value on the EQ?), or buyer beware?

The overall graphic design could use some work. And the code is still pretty messy.