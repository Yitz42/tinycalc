# Changelog

## 1.2.0

**Typing and editing**
- Long lines no longer shrink to tiny text: they stop at a readable size and scroll sideways, keeping what you're typing (or the cursor) in view. Small arrows show there's more. Drag sideways on the display to scroll; only holding still brings up the cursor.
- Brackets around the top or bottom of a fraction, or around a power, stay visible until you close them, so you can see you're still inside.
- With the cursor inside a fraction's number (1/23|4), the number stays together instead of looking like a multiplication.
- "=" can no longer land inside brackets: at the end it closes them first, in the middle of a bracket it's refused with a buzz. Before, it could pull the rest of a fraction out.

**Solving and graphing**
- An equation that holds for every x (2x = 2x) says "True for every x" (x/x = 1: "where it's defined") instead of "No roots found".
- Two equations that are the same curve (y = x and 2y = 2x) say "The same curve twice" instead of listing points on it.
- With more than 20 roots in range (sin 10x = 0), the list keeps the 20 nearest the middle, so 0 and its neighbours show.
- z is an unknown: Graph says "Graph: z isn't defined" instead of plotting with z's stored value, and an equation with z goes to the system solver.

**Display**
- A formula too tall for the small line (a fraction in a power) shows as one line of text there instead of running into the answer, in History too.

**Memory**
- The typesetter's scratch space now makes way along with it when the solver or another big part needs room.

## 1.1.0

First public release on the Pebble app store.
