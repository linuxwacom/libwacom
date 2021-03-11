# libwacom SVG format requirements

SVG images have a dual purpose, providing an accurate representation of the
tablets and also providing the size and location of the various controls on
the device that can be queried by various applications that may need it.

To allow applications to identify each control in the SVG and also apply a
CSS if desired, the following naming convention applies:

## Buttons

Each button ID in the SVG is built of the string "Button" with ID of the
button between 'A' and 'Z'. Additionally, the SVG class must contain the
button ID of the button between 'A' and 'Z' and the more generic class
"Button":

| Button ID |  SVG id   | SVG class  |
|:---------:|:---------:|:----------:|
|    `A`    | `ButtonA` | `A Button` |
|    `B`    | `ButtonB` | `B Button` |
|    ...    |   ...     |  ...
|    `Z`    | `ButtonZ` | `Z Button` |


For example:
```xml
    <path id="ButtonA" class="A Button"
       d="m 180.5,0.0 0,73.0 94.5,0 a 68.0,68.0 0 0 1 65.0,-67.5 l 0,-5.5 -160.0,0 z" />
```

If the button is a mode-switch button as well, the class list should also
contain "ModeSwitch":

```xml
    <path id="ButtonI" class="I ModeSwitch Button" ... />
```

This allows applications to modify the appearance of all the buttons at once
via a CSS, yet allowing to customize the appearance of single buttons by
using the button ID.

## Touch Rings/Strips

Touch rings and strips use the following convention:

| Type               |  SVG id  | SVG class          |
|--------------------|:--------:|:------------------:|
| First touch ring   | `Ring`   | `Ring TouchRing`    |
| Second touch ring  | `Ring2`  | `Ring2 TouchRing`   |
| First touch strip  | `Strip`  | `Strip TouchStrip`  |
| Second touch strip | `Strip2` | `Strip2 TouchStrip` |

For example:
```xml
    <circle id="Ring" class="Ring TouchRing" cx="342.0" cy="74.5" r="50.5" />
```

### Fake buttons to indicate interactions

Additional "fake" buttons (i.e. actual controls not found on the real
device) may be added to touch controls to help indicate the type of action
expected on the control. For example a circular motion arrow on a touch ring
or vertical motion arrow on a touch strip.

Where the controls are present, they should be named using the following
convention:

| Type               |  SVG id  | SVG class          |
|--------------------|:--------:|:------------------:|
| First touch ring, rotating clockwise          | `RingCW`     | `RingCW Button `    |
| First touch ring, rotating counterclockwise   | `RingCCW`    | `RingCCW Button`    |
| Second touch ring, rotating clockwise         | `Ring2CW`    | `Ring2CW Button `   |
| Second touch ring, rotating counterclockwise  | `Ring2CCW`   | `Ring2CCW Button`   |
| First touch strip, moving up                  | `StripUp`    | `StripUp Button`    |
| First touch strip, moving down                | `StripDown`  | `StripDown Button`  |
| Second touch strip, moving up                 | `Strip2Up`   | `Strip2Up Button`   |
| Second touch strip, moving down               | `Strip2Down` | `Strip2Down Button` |

For example:

```xml
  <g>
    <circle id="Ring" class="Ring TouchRing" cx="342.0" cy="74.5" r="50.5" />
    <path id="RingCCW" class="RingCW Button"
       d="m 372.5,103.0 -3.5,-3.5 c 17.5,-17.5 7.0,-42.5 3.5,-53.1 7.0,7.0 21.5,39.0 0,56.5 z" />
    <path id="RingDown" class="RingCCW Button"
       d="m 340.0,35.5 -3.5,7.0 7.0,0 z" />
    ...
```

The use of those "fake" buttons is left at the discretion of the designer
and is not mandatory nor enforced. Multiple controls may be present, e.g.
it's allowed to add both a CW and CCW motion indicator.


## Labels

The role of the labels in the SVG is to give applications an indication on
where to place the caption for each button. The actual content of the text
in the SVG image may not be relevant, what matters is the actual location.

Applications should hide the labels (using CSS) or replace the text with an
appropriate caption.


The convention is to prefix the type with "Label" for the SVG id and append
"Label" to the type for the SVG class:

| Type               |  SVG id  | SVG class          |
|--------------------|:--------:|:------------------:|
| Button 'A' label                                        | `LabelA`          | `A Label`                 |
| Button 'B' label                                        | `LabelB`          | `B Label`                 |
| Button 'C' label for a mode-switch button               | `LabelC`          | `C ModeSwitch Label`      |
| Label for first touch ring, rotating clockwise          | `LabelRingCW`     | `RingCCW Ring Label`      |
| Label for first touch ring, rotating counterclockwise   | `LabelRingCCW`    | `RingCCW Ring Label`      |
| Label for second touch ring, rotating clockwise         | `LabelRing2CW`    | `Ring2CCW Ring2 Label`    |
| Label for second touch ring, rotating counterclockwise  | `LabelRing2CCW`   | `Ring2CCW Ring2 Label`    |
| Label for first touch strip, moving up                  | `LabelStripUp`    | `StripUp Strip Label`     |
| Label for first touch strip, moving down                | `LabelStripDown`  | `StripDown Strip Label`   |
| Label for second touch strip, moving up                 | `LabelStrip2Up`   | `Strip2Up Strip2 Label`   |
| Label for second touch strip, moving down               | `LabelStrip2Down` | `Strip2Down Strip2 Label` |

For example:
```xml
    <text id="LabelD" class="D Label" x="534.0" y="115.0" style="text-anchor:start;">D</text>
    ...
    <text id="LabelRingCCW" class="RingCCW Ring Label" x="404.0" y="165.0" style="text-anchor:start;">CCW</text>
```

### Caption leader lines

To match the buttons with their corresponding labels, the SVG must also
provide a leader line for each label in the form of a line that links
each button and its label.

Each leader line follows the same naming convention as the labels, using
the special name "Leader" in place of "Label", ie:

| Type               |  SVG id  | SVG class          |
|--------------------|:--------:|:------------------:|
| Button 'A' leader                                        | `LeaderA`          | `A Leader`                 |
| Button 'B' leader                                        | `LeaderB`          | `B Leader`                 |
| Button 'C' leader for a mode-switch button               | `LeaderC`          | `C ModeSwitch Leader`      |
| Leader for first touch ring, rotating clockwise          | `LeaderRingCW`     | `RingCCW Ring Leader`      |
| Leader for first touch ring, rotating counterclockwise   | `LeaderRingCCW`    | `RingCCW Ring Leader`      |
| Leader for second touch ring, rotating clockwise         | `LeaderRing2CW`    | `Ring2CCW Ring2 Leader`    |
| Leader for second touch ring, rotating counterclockwise  | `LeaderRing2CCW`   | `Ring2CCW Ring2 Leader`    |
| Leader for first touch strip, moving up                  | `LeaderStripUp`    | `StripUp Strip Leader`     |
| Leader for first touch strip, moving down                | `LeaderStripDown`  | `StripDown Strip Leader`   |
| Leader for second touch strip, moving up                 | `LeaderStrip2Up`   | `Strip2Up Strip2 Leader`   |
| Leader for second touch strip, moving down               | `LeaderStrip2Down` | `Strip2Down Strip2 Leader` |

For example:

```xml
    <path id="LeaderRingCCW" class="RingCCW Ring Leader"
      d="m 372.5,110.0 l 0.0,55.0 l 30.0,0.0" />
```

## Tips For Creating New Layouts

Layouts use very simple SVG rules. WISIWYG editors such as Inkscape are
very convenient to design new layouts but usually produce a much more
complex SVG markup so files that are produced with those editors should
be cleaned. To help with this task, there is a script called `clean_svg.py`
in the tools folder.
Besides cleaning the markup and removing editor specific tags, `clean_svg.py`
also automates the naming of the elements.

### Automatic Naming with Inkscape and `clean_svg.py`

In Inkscape, be sure to group the button, leader and label elements
and assign the group's ID to the desired logical name. e.g.: Assigning
"A" to the group's ID and running the `clean_svg.py` script with that
SVG, will assign "ButtonA"/"B Button" to the ID and class of the first
rect/circle element found in the group; it also analogously assigns the ID
and class of the first path and text elements found in that group.

`clean_svg.py` needs two arguments, the SVG file path and the name of
the tablet, e.g.:

```
  ./clean_svg.py /path/to/svg_file.svg "My Brand New Tablet Name"
```
