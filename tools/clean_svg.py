#! /usr/bin/env python
#
# Copyright (c) 2013 Red Hat, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
# Author: Joaquim Rocha <jrocha@redhat.com>
#

import sys
from pathlib import Path
from argparse import ArgumentParser
from xml.etree import ElementTree as ET

NAMESPACE = "http://www.w3.org/2000/svg"
BRACKETS_NAMESPACE = "{" + NAMESPACE + "}"


def human_round(number):
    """
    Round to closest .5, keep integer values
    """
    v = round(number * 2) / 2.0
    return int(v) if v == int(v) else v


def traverse_and_clean(node):
    """
    Clean the tree recursively
    """
    # Remove any non-SVG namespace attributes
    for key in list(node.attrib.keys()):
        if key.startswith("{"):
            del node.attrib[key]
    if node.tag == "g" and "id" in node.attrib:
        apply_id_and_class_from_group(node)
        del node.attrib["id"]
    if "style" in node.attrib:
        if node.tag == "text":
            node.attrib["style"] = "text-anchor:start;"
        elif node.tag != "svg":
            del node.attrib["style"]

    remove_transform_if_exists(node)

    round_attrib(node, "d", "x", "y", "rx", "ry", "width", "height", "cx", "cy", "r")

    for child in node:
        traverse_and_clean(child)


def round_attrib(node, *attrs):
    for attr_name in attrs:
        attr_value = node.attrib.get(attr_name)
        if attr_value is None:
            continue
        if attr_name == "d":
            d = attr_value.replace(",", " ")
            values = [round_if_number(value) for value in d.split()]
            node.attrib[attr_name] = " ".join(values)
        else:
            node.attrib[attr_name] = round_if_number(attr_value)


def round_if_number(value):
    try:
        value = human_round(float(value.strip()))
    except ValueError:
        pass
    return str(value)


def remove_non_svg_nodes_and_strip_namespace(root):
    if root.tag.startswith(BRACKETS_NAMESPACE):
        root.tag = root.tag[len(BRACKETS_NAMESPACE) :]
    for elem in root:
        if (
            not elem.tag.startswith(BRACKETS_NAMESPACE)
            or elem.tag == BRACKETS_NAMESPACE + "metadata"
        ):
            root.remove(elem)
        else:
            remove_non_svg_nodes_and_strip_namespace(elem)


def remove_transform_if_exists(node):
    TRANSLATE = "translate"
    MATRIX = "matrix"

    transform = node.attrib.get("transform")
    if transform is None:
        return
    transform = transform.strip()

    if transform.startswith(TRANSLATE):
        values = transform[len(TRANSLATE) + 1 : -1].split(",")
        try:
            x, y = float(values[0]), float(values[1])
        except Exception:
            return

        apply_translation(node, 1, 0, 0, 1, x, y)
    elif transform.startswith(MATRIX):
        values = transform[len(MATRIX) + 1 : -1].split(",")
        try:
            a, b, c, d, e, f = [float(value.strip()) for value in values]
        except Exception:
            return
        apply_translation(node, a, b, c, d, e, f)
        apply_scaling(node, a, d)
    del node.attrib["transform"]


def apply_translation(node, a, b, c, d, e, f):
    x_attr, y_attr = "x", "y"
    if node.tag == "circle":
        x_attr, y_attr = "cx", "cy"
    elif node.tag == "path":
        apply_translation_to_path(node, e, f)
        return
    try:
        x, y = float(node.attrib[x_attr]), float(node.attrib[y_attr])
        new_x = x * a + y * c + 1 * e
        new_y = x * b + y * d + 1 * f
        node.attrib[x_attr] = str(new_x)
        node.attrib[y_attr] = str(new_y)
    except Exception:
        pass


def apply_translation_to_path(node, x, y):
    d = node.attrib.get("d")
    if d is None:
        return
    d = d.replace(",", " ").split()
    m_init_index = 0
    length = len(d)
    m_end_index = length
    operation = "M"
    i = 0
    while i < length:
        value = d[i]
        if value.lower() == "m":
            operation = value
            m_init_index = i + 1
        elif len(value) == 1 and value.isalpha():
            m_end_index = i
            break
        i += 1
    for i in range(m_init_index, m_end_index, 2):
        d[i] = str(float(d[i]) + x)
        d[i + 1] = str(float(d[i + 1]) + y)
        if operation == "m":
            break
    node.attrib["d"] = (
        " ".join(d[:m_init_index])
        + " "
        + " ".join(d[m_init_index:m_end_index])
        + " ".join(d[m_end_index:])
    )


def apply_scaling(node, x, y):
    w_attr, h_attr = "width", "height"
    if node.tag == "circle":
        r = float(node.attrib.get("r", 1.0))
        node.attrib["r"] = str(r * x)
    try:
        w = float(node.attrib[w_attr])
        h = float(node.attrib[h_attr])
        node.attrib[w_attr] = str(w * x)
        node.attrib[h_attr] = str(h * y)
    except Exception:
        pass


def to_string_rec(node, level=0):
    indent = "\n" + level * "  "

    tag_name = node.tag

    # Remove 'defs' element. This cannot be done in the traverse_and_clean
    # because somehow it is never found
    if tag_name == "defs":
        return ""

    # use a list to put id and class as the first arguments
    attribs = []
    for attr in get_node_attrs_sorted(node):
        attr_value = node.attrib.get(attr)
        if attr_value is not None:
            attribs.append(indent + '   %s="%s"' % (attr, attr_value))

    string = indent + "<" + tag_name + "".join(attribs)
    if len(node) or node.text:
        string += ">"
        if not node.text or not node.text.strip():
            node.text = indent + "  "
        else:
            string += node.text
        if list(node):
            for child in get_node_children_sorted(node):
                string += to_string_rec(child, level + 1)
            string += indent
        string += "</%s>" % tag_name
    else:
        string += " />"
    return string


def custom_tag_sort(arg):
    """
    Use as key functon in sorted().

    Pre-fix arg tag name by a number in the sort order we want. Anything
    unspecified defaults to 9. i.e. circle -> 1circle, thus sorts lower
    than
    other tags.
    """
    tag_order = {"title": 0, "rect": 1, "circle": 2, "path": 3}
    return str(tag_order.get(arg.tag, 9)) + arg.tag


def get_node_children_sorted(node):
    return sorted(node, key=custom_tag_sort)


def custom_attr_sort(arg):
    """
    Same as custom_tag_sort but for a node's attributes
    """
    attr_order = {
        "id": 0,
        "class": 1,
        "x": 2,
        "y": 3,
        "cx": 4,
        "cy": 5,
        "width": 6,
        "height": 7,
    }
    return str(attr_order.get(arg, 9)) + arg


def get_node_attrs_sorted(node):
    attrs = node.attrib.keys()
    return sorted(attrs, key=custom_attr_sort)


def apply_id_and_class_from_group(group_node):
    button_assigned = label_assigned = path_assigned = False
    _id = group_node.attrib.get("id")
    if _id is None:
        return
    for child in group_node:
        if child.tag == "rect" or child.tag == "circle":
            if button_assigned:
                continue
            child.attrib["id"] = "Button%s" % _id
            child.attrib["class"] = "%s Button" % _id
            button_assigned = True
        elif child.tag == "path":
            if path_assigned:
                continue
            child.attrib["id"] = "Leader%s" % _id
            child.attrib["class"] = "%s Leader" % _id
            path_assigned = True
        elif child.tag == "text":
            if label_assigned:
                continue
            child.attrib["id"] = "Label%s" % _id
            child.attrib["class"] = "%s Label" % _id
            child.text = _id
            label_assigned = True


def to_string(root):
    header = """<?xml version="1.0" standalone="no"?>
<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN"
   "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">"""
    return header + to_string_rec(root)


def clean_svg(root, tabletname):
    remove_non_svg_nodes_and_strip_namespace(root)
    title = root.find("title")
    if title is not None:
        title.text = tabletname
    root.attrib["xmlns"] = "http://www.w3.org/2000/svg"
    traverse_and_clean(root)


if __name__ == "__main__":
    parser = ArgumentParser(description="Clean SVG files for libwacom")
    parser.add_argument(
        "--ignore-missing", action="store_true", default=False, help="Ignore .tablet files without a Layout"
    )
    parser.add_argument(
        "filename", type=str, help="SVG file to clean", metavar="FILE"
    )
    parser.add_argument(
        "tabletname",
        type=str,
        help="The name of the tablet",
        metavar="TABLET_NAME",
    )
    args = parser.parse_args()

    svgfile = args.filename
    tabletname = args.tabletname
    if args.filename.endswith(".tablet"):
        import configparser
        config = configparser.ConfigParser()
        config.read(args.filename)
        try:
            svgname = config["Device"]["Layout"]
        except KeyError:
            print(f"{args.filename} does not specify a layout, skipping", file=sys.stderr)
            sys.exit(0 if args.ignore_missing else 77)
        svgfile = Path(args.filename).parent / "layouts" / svgname
        tabletname = config["Device"]["Name"]


    ET.register_namespace("", NAMESPACE)
    try:
        tree = ET.parse(svgfile)
    except Exception as e:
        sys.stderr.write(str(e) + "\n")
        sys.exit(1)
    root = tree.getroot()
    clean_svg(root, tabletname)
    print(to_string(root))
