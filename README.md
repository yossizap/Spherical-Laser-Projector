# Spherical Laser Projector
This repository was created for an arduino based laser projector centered within a spherical lampshade colored with [photochromic](https://en.wikipedia.org/wiki/Photochromism) paint. We were able to draw fading images on the sphere from within by using an ultra violet laser which is able to activate the photochromatic dye for short periods of time.

The project was done for and in collaboration with the [Bloomfield Science Museum in Jerusalem](http://www.mada.org.il/en) that provided us with the hardware and photochromatic paint. It was created to serve as a science exhibit that demonstrates the properties of photochromatic dyes with basic svg drawings so keep in mind that more advanced projector capabilities might be missing from the code. The code is licensed with the MIT license so you are free to use the code for your own needs.

### Repository content
[/svgs/](https://github.com/yossizap/Spherical-Laser-Projector/tree/master/svgs) - SVGs that we have created for the projector.

[/src/projector/](https://github.com/yossizap/Spherical-Laser-Projector/tree/master/src/projector) - The arduino's code.

[/src/utils/](https://github.com/yossizap/Spherical-Laser-Projector/tree/master/src/utils) - Utilities used for projector-compatible code generation from SVG 1.1 files.

### Usage
Before uploading the projector.ino file onto an arduio you will have to create a folder named 'gen' in `/src/projector/` and run `utils/generate_paths.bat` to generate code from the svgs in `/svgs/` directory.

To add new svgs simply place your svg files in the `/svgs/` directory and add their filename with a `_DRAW` suffix to the drawings array in projector.ino in a `{drawing name, starting x position, starting y position, scale}` format. Removing svgs requires the opposite.
