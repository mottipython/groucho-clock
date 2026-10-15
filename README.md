# The Groucho Clock

A cartoon screensaver clock: Groucho Marx tells the time with his eyes. Runs on
Linux, Windows and macOS. One of the
[Marx Brothers screensavers](https://github.com/mottipython/marx-screensavers).

![Seven fifty-two](docs/screenshots/01-seven-fifty-two.jpg)

## What it shows

- **The time, in his glasses.** Each lens is a clock face: hours on the left, minutes
  on the right. His pupils are at the tips of the hands.
- **The seconds**, on his cigar, which snaps up and down a notch every second.
- **Every minute** his eyebrows waggle.
- **Through the hour** the cigar burns down and the ash grows. On the hour the ash
  breaks off and falls, and he lights a new one.
- **Every quarter of an hour** he grins and holds up one of his own lines on a card:
  51 of them, from *The Cocoanuts* to *A Day at the Races*, in a different order every
  day, each with the film it is from.
- **Now and then** his brother Gummo, who left the act before the films, peeks in.

| | |
|---|---|
| ![](docs/screenshots/02-five-oclock.jpg) | ![](docs/screenshots/03-gummo.jpg) |
| Five o'clock | Gummo |

## Running it yourself

    groucho-clock                  full screen on every display
    groucho-clock --window         in a window; Esc or Q quits
    groucho-clock --mute           without the sound
    groucho-clock --clock 4:59:50  start at that time of day instead of the real time
    groucho-clock --snapshot out.bmp --time 3 --size 1920x1080

## How it was made

The cartoon pictures were drawn with Google's Gemini from the prompts in
`assets/PROMPTS.md` (the sheets as Gemini made them are in `assets/src`), then cut up
by `tools/make_parts.py` and put together and animated in code. The program was
written with Anthropic's Claude, working from the author's direction. The lines are
Groucho's own, as Wikiquote gives them.

## Building

Needs a C compiler, make and SDL2 (2.0.18 or newer). Build from the top of the source
tree, because the pictures in `assets/art` are built into the program.

    make

Packages are written to `dist/`, using the container in `tools/Containerfile`:

    podman build -t anathem-build -f tools/Containerfile tools
    podman run --rm --security-opt label=disable -v "$PWD":/src -w /src anathem-build make dist

## Credits and licence

The code is MIT. An unofficial, non-commercial homage, not connected with the Marx
Brothers' estate or the studios. Images are decoded with
[stb_image](https://github.com/nothings/stb) (public domain).
