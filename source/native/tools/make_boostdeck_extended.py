"""Generate BoostDeck gauge layout and replace the background in its image archive."""
import argparse
from io import BytesIO
from pathlib import Path
import re
import xml.etree.ElementTree as ET
from zipfile import ZIP_DEFLATED, ZipFile

ROOT = Path(__file__).resolve().parents[1]
XML = ROOT / "plugin/BoostDeck.xml"
SIGNALS = re.findall(r'^SIGNAL\(\w+, "([^"]+)"',
                     (ROOT / "src/boost_signals.def").read_text(), re.M)
SLOTS = 9
BACKGROUND = 56300
BAR_EMPTY = 55000
WIDTH, HEIGHT = 1172, 400
ETHANOL_EMPTY = 56100

# Original four columns of two tiles and the full-height ninth tile.
def tile(i):
    return (258 * (i // 2), 208 * (i % 2)) if i < 8 else (1032, 0)


def placed(parent, tag, identifier, model, x, y, w, h):
    node = ET.SubElement(parent, tag, id=str(identifier), model=str(model))
    props = ET.SubElement(node, "properties")
    for prop, value in ((20, x + 24), (21, y + 13), (9, w), (10, h)):
        ET.SubElement(props, "property", id=str(prop), value=str(value))
    if tag == "image":
        for prop, value in ((2, "false"), (3, "true")):
            ET.SubElement(props, "property", id=str(prop), value=value)


def update_xml():
    tree = ET.parse(XML)
    app = tree.find("pluginApp")
    actions, models, states = (app.find(name) for name in ("actions", "models", "hmiStates"))
    for entry in list(actions):
        if int(entry.get("id")) >= 500:
            actions.remove(entry)
    for entry in list(models):
        if entry.get("id") != "110":
            models.remove(entry)
    for state in list(states):
        if int(state.get("id")) >= 500:
            states.remove(state)
    gauges = states.find("hmiState[@id='100']")
    for child in list(gauges):
        gauges.remove(child)
    opts = ET.SubElement(gauges, "optionComponents")
    for identifier, action, caption in ((33, 31, "Gauge color"), (410, 49, "Units Imperial"),
                                         (411, 59, "Units Metric"), (412, 500, "Configure gauges"),
                                         (413, 501, "Engine profile")):
        ET.SubElement(opts, "button", id=str(identifier), action=str(action), text=caption)
    components = ET.SubElement(gauges, "components")
    ET.SubElement(models, "imageIdModel", id="69", imageId=str(BACKGROUND))
    placed(components, "image", 209, 69, 0, 0, WIDTH, HEIGHT)
    for action, page in ((500, 500), (501, 501)):
        ET.SubElement(actions, "hmiAction", id=str(action), target=str(page))
    for action in (502, 503, *range(600, 600 + len(SIGNALS))):
        ET.SubElement(actions, "raAction", id=str(action))
    config = ET.SubElement(states, "hmiState", id="500", parentId="100", text="Configure gauges")
    links = ET.SubElement(config, "components")
    profile = ET.SubElement(states, "hmiState", id="501", parentId="100", text="Engine profile")
    choices = ET.SubElement(profile, "components")
    ET.SubElement(models, "raDataModel", id="360", modelType="Richtext")
    ET.SubElement(choices, "label", id="590", model="360")
    ET.SubElement(choices, "button", id="591", action="502", text="Gasoline")
    ET.SubElement(choices, "button", id="592", action="503", text="Diesel (standard OBD PIDs)")

    for i in range(SLOTS):
        x, y = tile(i)
        for model in (300 + i, 340 + i, 370 + i):
            ET.SubElement(models, "raDataModel", id=str(model), modelType="Richtext")
        empty = BAR_EMPTY if i < 8 else ETHANOL_EMPTY
        ET.SubElement(models, "imageIdModel", id=str(320 + i), imageId=str(empty))
        if i < 8:
            placed(components, "label", 260+i, 370+i, x+15, y+16, 216, 40)
            placed(components, "label", 310+i, 300+i, x+15, y+60, 216, 66)
            placed(components, "image", 280+i, 320+i, x+15, y+140, 210, 15)
        else:
            placed(components, "label", 260+i, 370+i, x+10, y+10, 120, 46)
            placed(components, "image", 280+i, 320+i, x+50, y+58, 40, 250)
            placed(components, "label", 310+i, 300+i, x+24, y+322, 112, 63)
        target = str(510 + i)
        ET.SubElement(actions, "hmiAction", id=target, target=target)
        ET.SubElement(links, "button", id=str(540 + i), action=target, text=f"Gauge {i+1}")
        page = ET.SubElement(states, "hmiState", id=target, parentId="500", text=f"Gauge {i+1}")
        buttons = ET.SubElement(page, "components")
        base = 1000 + i * (len(SIGNALS) + 1)
        ET.SubElement(buttons, "label", id=str(base), model=str(340 + i))
        for j, caption in enumerate(SIGNALS):
            ET.SubElement(buttons, "button", id=str(base + j + 1), action=str(600 + j), text=caption)
    ET.indent(tree, space="  ")
    tree.write(XML, encoding="utf-8", xml_declaration=True)


def update_images(source, dest):
    from PIL import Image, ImageDraw
    with ZipFile(source) as original, ZipFile(dest, "w", ZIP_DEFLATED) as updated:
        background_name = f"{BACKGROUND}.png"
        background = Image.open(BytesIO(original.read(background_name))).convert("RGBA")
        if background.size != (WIDTH, HEIGHT):
            raise ValueError(f"Unexpected BoostDeck background size: {background.size}")
        draw = ImageDraw.Draw(background)
        for i in range(SLOTS):
            x, y = tile(i)
            # Replace only the old painted caption. The plugin supplies the current signal name.
            right = 232 if i < 8 else 132
            draw.rectangle((x+8, y+13, x+right, y+51), fill=background.getpixel((x+24,y+65)))
        bitmap = BytesIO()
        background.save(bitmap, format="PNG", optimize=True)
        for entry in original.infolist():
            data = bitmap.getvalue() if entry.filename == background_name else original.read(entry)
            updated.writestr(entry, data)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--images", type=Path)
    parser.add_argument("--images-out", type=Path)
    args = parser.parse_args()
    if bool(args.images) != bool(args.images_out) or (args.images and args.images == args.images_out):
        parser.error("provide distinct --images and --images-out paths")
    update_xml()
    if args.images:
        update_images(args.images, args.images_out)
