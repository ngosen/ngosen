# Web spreadsheet test pages

These pages load JavaScript spreadsheets and report their cells to a small local server. `run.sh`
types Telex one word per cell, moves between cells the way people do, and compares what the cells
hold with the expected words. Moving to another cell is where sheets differ from plain editors: the
cell editor is replaced, and the app may report the new cell late.

| Page | Spreadsheet | Arrows end an edit |
| --- | --- | --- |
| `handsontable` | Handsontable 14, a cell editor in a `<textarea>` | yes |
| `jspreadsheet` | jspreadsheet CE 4 | no |
| `xspreadsheet` | x-data-spreadsheet, drawn on a canvas | no |
| `aggrid` | AG Grid 32 with editable cells | no |
| `luckysheet` | Luckysheet, a `contenteditable` cell editor | yes |
| `univer` | Univer 0.10, canvas with its own text editor, close to Google Sheets | yes |

The pages load pinned versions from jsDelivr, so you need a network connection. The local server is
`misc/editor-test/server.py`.

## Running

Run it inside an X11 session, with Ngó Sen on in Gõ Sen mode:

```sh
misc/sheet-test/run.sh                               # every page, every case, Firefox
BROWSER=chromium misc/sheet-test/run.sh univer luckysheet
CASES=down RUNS=10 misc/sheet-test/run.sh handsontable
```

The script needs `python3`, `xdotool` and Firefox or Chromium. It opens the browser in kiosk mode
with a fresh profile for each run, clicks cell A1, then types. The cases are:

| Case | Keys | Expected cells |
| --- | --- | --- |
| `down` | `tieengs`, Down, `vieetj`, Down, … `caau`, Down | `tiếng` `việt` `nhận` `đây` `là` `câu` down column A |
| `enter` | the same words, Return after each | the same |
| `tab` | `tieengs`, Tab, `vieetj`, Tab, … `laf`, Return | `tiếng` `việt` `nhận` `đây` `là` along row 1 |
| `fast` | same as `enter`, 30 ms between keys | the same |
| `click` | `tieengs`, click on A3, `vieetj`, Return | `tiếng` in A1, `việt` in A3 |
| `back` | `toan`, space, BackSpace, `s`, Return | `toán` in A1 |
| `plainclick` | `abc`, click on A3, `xyz`, Return | `abc` in A1, `xyz` in A3 |

`down` is skipped on sheets where arrows only move the caret inside the cell. `plainclick` is not in
the default set: it types no Vietnamese, so a failure there is the sheet's own behaviour (Univer keeps
editing A1 when A3 is clicked).

## Running in a VM (Wayland too)

`xdotool` does not work on Wayland. With `VM` set, `run.sh` runs on a libvirt host instead and sends
the keys with `virsh send-key` and the clicks with QMP `input-send-event`, which reach the guest like a
real keyboard and mouse. Copy `misc/sheet-test` and `misc/editor-test/server.py` into the guest (by
default under `/tmp/ngosen-kit/misc`), then:

```sh
VM=ubuntu2404 GUEST="ssh root@192.168.122.50" BROWSER=chromium CHROMIUM=google-chrome \
    OPEN="bash /tmp/ngosen-kit/misc/sheet-test/in-session.sh ubuntu gnome-shell" misc/sheet-test/run.sh
```

`GUEST` runs a shell command in the guest as root; the test server and the report stay in the guest.
`OPEN` starts the browser in the desktop session: `in-session.sh` copies the environment of a session
process (`gnome-shell`, `plasmashell`, `cinnamon`) and starts the program as a transient systemd unit.
Each `virsh send-key` call takes a few tens of milliseconds, so the `fast` case is slower than 30 ms
per key there.

## Writing a page

Load `report.js` and call `report(name, getCells, cellCenter)`. `getCells` returns the cell values
as rows, or `null` while the sheet is loading. `cellCenter(row, col)` returns the cell's centre in
page coordinates, which the runner uses to click A1 and A3. Set `window.arrowsCommit = true` when an
arrow key ends the edit and moves to the next cell.
