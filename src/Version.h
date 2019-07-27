#pragma once

//static const char* szVEROROUTE_VERSION = "0.91  (beta)";	// Fixed bad write to pdf. Allow backspace to work as delete.
//static const char* szVEROROUTE_VERSION = "0.92  (beta)";	// Add Ctrl shortcuts for File menu options. Changed tutorials.  Fixed origId bug.
//static const char* szVEROROUTE_VERSION = "0.93  (beta)";	// Try auto-generate unique Name when adding new part.
//static const char* szVEROROUTE_VERSION = "0.94  (beta)";	// Added B.O.M.
//static const char* szVEROROUTE_VERSION = "0.95  (beta)";	// Added save to PNG.
//static const char* szVEROROUTE_VERSION = "0.96  (beta)";	// Show File menu-shortcuts.  Fixed tutorial description of polarised component Export.
//static const char* szVEROROUTE_VERSION = "0.97  (beta)";	// Added new part types.  Improved TO-92 outline.  Auto-centre when writing to PDF.
//static const char* szVEROROUTE_VERSION = "0.98  (beta)";	// Added Undo/Redo.
//static const char* szVEROROUTE_VERSION = "0.981 (beta)";	// Bug fix: Undo/Redo.
//static const char* szVEROROUTE_VERSION = "0.99  (beta)";	// Added new part types including multi-pole switches.
//static const char* szVEROROUTE_VERSION = "0.991 (beta)";	// Bug fix: Redraw when toggling track styles.
//static const char* szVEROROUTE_VERSION = "0.992 (beta)";	// Improve routing.
//static const char* szVEROROUTE_VERSION = "0.993 (beta)";	// Routing improvement.
//static const char* szVEROROUTE_VERSION = "0.994 (beta)";	// Bug fix: Erasing under floating pins while auto-routing.
//static const char* szVEROROUTE_VERSION = "0.995 (beta)";	// Check if circuit has been saved before launching Tutorials. Don't auto-raise Info window on Undo/Redo.
//static const char* szVEROROUTE_VERSION = "0.996 (beta)";	// Bug fix: Deleting grouped components could cause a crash later. Allow DIP gap resizing with "E","R" keys.
//static const char* szVEROROUTE_VERSION = "0.997 (beta)";	// Added part type combo.  Added width buttons for DIP gap resizing instead of "E","R" keys.
//static const char* szVEROROUTE_VERSION = "0.998 (beta)";	// Bug fix: After "Paste" auto-routed wire, color should stay on board when wire moved.
//static const char* szVEROROUTE_VERSION = "0.999 (beta)";	// Bug fix: Fix possible crash if adding new wire while auto-routing is enabled.
//static const char* szVEROROUTE_VERSION = "1.00";			// Disable component text buttons when appropriate.  Tidy code.  Drop Beta.
//static const char* szVEROROUTE_VERSION = "1.01";			// Added scroll bars. Place new components in top-left of visible view and float them if needed.
//static const char* szVEROROUTE_VERSION = "1.10";			// Added import of netlist from TinyCAD schematic. Hide old Import/Export.
//static const char* szVEROROUTE_VERSION = "1.11";			// Fixed crash on importing unknown package. Increased maximum number of pins per component to over 200.
//static const char* szVEROROUTE_VERSION = "1.12";			// Fixed TinyCAD package names for NP capacitors. Make netlist import allow spaces in Name and Value fields.
//static const char* szVEROROUTE_VERSION = "1.13";			// Bug fixes: PDF write was broken since V1.10.  "Add" menu item was enabled when view was mirrored.
//static const char* szVEROROUTE_VERSION = "1.14";			// Added Help menu item to check if a new version is available.
//static const char* szVEROROUTE_VERSION = "1.15";			// Added more Edit menu items.  Check for Tutorials and History folders at start-up.
//static const char* szVEROROUTE_VERSION = "1.16";			// Fixed mouse-wheel behaviour on windows.  Added View menu option to show IC pins as numbers.
//static const char* szVEROROUTE_VERSION = "1.17";			// Added ground-fill.
//static const char* szVEROROUTE_VERSION = "1.18";			// Bug fix: Info dialog edits weren't treated as changes so "Save" was broken, but "Save As" was OK.
//static const char* szVEROROUTE_VERSION = "1.19";			// Add separate "Paste" and "Paste+Tidy" buttons for auto-routing.  Updated tutorials.
//static const char* szVEROROUTE_VERSION = "1.20";			// Added Grid checkbox.  PDF shows bounding box and can show grid.
//															// Increased max pins per component to 255.
//															// When writing to file, append ". vrt", ".pdf", ".png" to filename as needed.
//static const char* szVEROROUTE_VERSION = "1.21";			// Improved rendering speed by avoiding pointless repaints.
//															// Control dialog rearranged so a checkbox controls ground-fill.
//															// Draw foil capacitors with thinner shape to better match the footprint.
//															// Draw floating component text in red.
//															// Made export to PDF centre on grid bounds instead of circuit bounds.
//															// Write status bar messages.
//static const char* szVEROROUTE_VERSION = "1.22";			// Added command line options to specify vrt file and path to VeroRoute home directory.
//															// Support drag and drop of vrt files from file explorer.
//															// Allow multiple VeroRoute instances to run at the same time.
//															// Improved error reporting during netlist import.
//															// Added color saturation slider.
//															// Changed rendering behaviour when clicking on an item in the Broken list.
//															// Added gEDA symbol libraries.
//static const char* szVEROROUTE_VERSION = "1.23";			// Fixed memory leak on shutdown.
//															// Updated Tutorial 19 with info on creating a registry key for MS Windows.
//															// Added more JFETs to the symbol libraries.
//static const char* szVEROROUTE_VERSION = "1.24";			// Added IC Pin Labels dialog.
//static const char* szVEROROUTE_VERSION = "1.25";			// Usability tweaks:
//															// Slow down the grid auto-pan/resize when moving components with the mouse.
//															// Selecting a NodeID in the "Broken" list should also highlight it in the "Floating" list if it's there.
//															// Added a Spin FV-1 IC and serial EEPROM to the gEDA library.
//static const char* szVEROROUTE_VERSION = "1.26";			// Bug fix: Holding down "P" and SPACE at the same time would allow mouse move to modify component pins.
//															// Usability tweak: Holding down SPACE now allows un-painting the board under a placed component pin,
//															// or painting the board under the pin to match the existing NodeId on the pin.
//static const char* szVEROROUTE_VERSION = "1.27";			// Bug fix:  Deleting a component was not immediately updating the Bad Nodes lists.
//															// Bug fix:  V1.26 allowed components to be dragged while user was painting pins.
//static const char* szVEROROUTE_VERSION = "1.28";			// Improvements:
//															// Added "Toggle Grid" and "Toggle Mirror" as View menu items.
//															// List part types in B.O.M.
//															// Allow pin labels for TO packages as well as DIPs, SIPs.
//															// New feature: Added "Parts/Templates Dialog" so user can build a parts library
//															// for ICs and TO packages (avoids defining pin labels with each new circuit).
//															// Double-clicking on any listed part adds it to the circuit.
//static const char* szVEROROUTE_VERSION = "1.29";			// Added Crystal component type and gEDA library symbol.
//static const char* szVEROROUTE_VERSION = "1.30";			// Improvement: Duplicating components copies their Value too if it's different to the name.
//															// Added more component types. (Relays, Switches, Trimpots).
//															// Changed default lengths of resistors and diodes to 400 mil.
//															// Updated gEDA/TinyCAD import code for RESISTOR/DIODE/CAP_CERAMIC/CAP_FILM
//															// so that appending an optional integer number to the footprint name
//															// sets the length in 100s of mil. (e.g. RESISTOR3 = 300mil)
//															// Updated gEDA library with resistors and caps of different lengths.
//															// Added ability to select components/wipe tracks within area (hold down "R" to draw areas).
//static const char* szVEROROUTE_VERSION = "1.31";			// Improvement: User-defined areas select tracks as well as components.
//static const char* szVEROROUTE_VERSION = "1.32";			// New feature: "File->Open (merge into current)".
//static const char* szVEROROUTE_VERSION = "1.33";			// New feature: Allow horizontal veroboard strips.
//															// Tweaked color assignment algorithm.
//static const char* szVEROROUTE_VERSION = "1.34";			// Bug fix:  Merge was not clearing existing user-defined areas.
//															// New feature: Added margin control for auto-crop.
//															// New feature: While the user draws a rectangle, show its size in the status bar.
//static const char* szVEROROUTE_VERSION = "1.35";			// New feature: Replace "Mirror" with "Flip-H" and "Flip-V" options.
//															// New feature: Add->Text menu item for putting labels on the grid.
//static const char* szVEROROUTE_VERSION = "1.36";			// Bug fix: Buttons in Text Label Editor weren't always reflecting text style.
//															// Bug fix: Prevent user from selecting text boxes in "Hide Text" mode.
//															// New feature: Added painting by "flood-fill".
//static const char* szVEROROUTE_VERSION = "1.37";			// Bug fix: Trying to undo "Delete text" could cause a crash.
//															// New feature: Text color can now be chosen.
//															// New feature: Hitting "V" key will copy+paste a selected text box.
//static const char* szVEROROUTE_VERSION = "1.38";			// Added more component types: TO-39 package, pin strips, terminal blocks, inductor, wide film cap, fuse holder, relays.
//static const char* szVEROROUTE_VERSION = "1.39";			// Moved some controls from the control dialog to the toolbar.  Reduced heights of control dialog and templates dialog.
//static const char* szVEROROUTE_VERSION = "1.40";			// Minor improvements: Automatically delete textboxes with no text.
//															// Show board size in mm in the window title bar.  Show drawn rectangle size in mm in the status bar.
//															// Color toolbar icons to differentiate track styles / diagonal modes.
//static const char* szVEROROUTE_VERSION = "1.41";			// Bug fix: Menu and toolbars were being blanked while drawing rectangles with the "R" key.
//															// Bug fix: The status of the diagonals mode was not always restored when toggling between vero and non-vero styles.
//															// Improvement: Prevent copying of textboxes with no text.
//															// Improvement: Show text box outline in dark grey instead of black, so it can be seen in ground-fill mode.
//															// Improvement: When zooming in/out, try to keep the view showing the same centre point.
//															// Improvement: Draw pin labels in black instead of grey so they are easier to read.
//static const char* szVEROROUTE_VERSION = "1.42";			// Improvement: Added Rendering dialog controls for component text size.
//															// Bug fix: Correct footprints for GTR-2 relays.
//															// Improved outline sizes for relays and Bourns trimpots.
//static const char* szVEROROUTE_VERSION = "1.43";			// Bug fix: Stop potential crash when defining areas for part/track selection.
															// Improvement: Add toolbar button to select part/tracks by area.
//static const char* szVEROROUTE_VERSION = "1.44";			// Bug fix: Fixed bad TRCD Relay.
//															// New feature: Added component editor.
//static const char* szVEROROUTE_VERSION = "1.45";			// Bug fix: Prevent mouse from dragging shapes off screen in the component editor.
//															// Bug fix: Fix shape selection in the component editor.
//static const char* szVEROROUTE_VERSION = "1.46";			// Bug fix: Fixed error in loaded old VRTs with strip connectors.
//static const char* szVEROROUTE_VERSION = "1.47";			// Bug fix: Version 1.46 would produce duplicate shapes when loading parts from VRT.
//															// Bug fix: Changing DIP width wasn't changing the component outline.
//															// New feature: Component editor can define "holes" (i.e. non-paintable grid points).
//static const char* szVEROROUTE_VERSION = "1.48";			// Bug fix: Possible crash during auto-routing while moving wires.
//															// Improvement: Speed up algorithm for routing/connectivity checking.
//static const char* szVEROROUTE_VERSION = "1.49";			// Restrict labels to 2 orientations. (Left to right, or bottom to top).
//															// Improvement: Add rendering options to show target board area.
//static const char* szVEROROUTE_VERSION = "1.50";			// Bug fix: Clicking on a floating track pattern should not place floating components.
//															// Moved the Pad/Track/Hole/Gap controls to the Rendering Options dialog.  Updated the tutorials.
//static const char* szVEROROUTE_VERSION = "1.51";			// Bug fix: Floating track pattern could not be placed over unpainted wires on the board.
//															// Improvement: Made buttons to move component labels use a smaller step size.
//static const char* szVEROROUTE_VERSION = "1.52";			// Bug fix: Missing Templates folder would disable Undo/Redo.
//															// Bug fix: Changing component type using Control Dialog (e.g. from Diode to LED) not working properly.
//															// History and Templates folders now auto-created in OS/application specific locations.
//static const char* szVEROROUTE_VERSION = "1.53";			// Improvement: Added Undo/Redo buttons to toolbar.
//															// Improvement: Allow Undo/Redo in Component Editor mode.
//static const char* szVEROROUTE_VERSION = "1.54";			// Added Zoom buttons to toolbar.
//															// Changed executable name, folder names, and file names to lowercase.
//															// Made program search for "tutorials" folder in standard locations.
//static const char* szVEROROUTE_VERSION = "1.55";			// Improvement: List recent VRT files in File menu.
//static const char* szVEROROUTE_VERSION = "1.56";			// Tweaked dialog layouts to better handle 11 point fonts.
//static const char* szVEROROUTE_VERSION = "1.57";			// Improvement: Faster routing algorithm.
//static const char* szVEROROUTE_VERSION = "1.58";			// Improvement: Faster routing and connectivity checking.
//static const char* szVEROROUTE_VERSION = "1.59";			// Improvement: Added Key/Mouse Actions dialog under Help menu.
static const char* szVEROROUTE_VERSION = "1.60";			// Bug fix: Since V1.58, connected tracks were not always rendered properly.
															// Improvement: Added option to disable "Fast" routing (to try reduce Bad Nodes).
