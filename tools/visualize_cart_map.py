import matplotlib.pyplot as plt

import matplotlib
import re
from collections import defaultdict
matplotlib.use("QT5Agg")

import re

g_freeSpace = 0

def evaluate_expression(expression, symbols):
    parts = expression.split("-")
    left = evaluate_term(parts[0].strip(), symbols)
    right = evaluate_term(parts[1].strip(), symbols)
    return left - right

def evaluate_term(term, symbols):
    if term.startswith("$"):
        return int(term[1:], 16)
    if term in symbols:
        return symbols[term]
    raise ValueError("Invalid expression: " + term)

def parse_value(value, symbols):

    # Replace all symbols with their values
    for key, val in symbols.items():
        v = val['value']
        value = value.replace(key, v)

    # Check if it's an expression
    if "-" in value:
        return evaluate_expression(value, symbols)
    # Check if it's a hexadecimal string
    if value.startswith("$"):
        return int(value[1:], 16)
    # Check if it's a predefined label
    if value in symbols:
        return symbols[value]

    return int(value)
    # Raise an error if it's neither an expression, a hexadecimal string, nor a predefined label
    raise ValueError("Invalid value: " + value)

def parse_memory_layout(layout):

    symbols = {}
    memory = {}
    with open(layout, "r") as f:
        layout = f.read()

    
    lines = layout.split("\n")
    current_section = None
    
    for line in lines:
        if "SYMBOLS {" in line:
            current_section = "symbols"
            continue
        elif "MEMORY {" in line:
            current_section = "memory"
            continue
        elif "}" in line:
            current_section = None
            continue
        elif current_section == "symbols":
            parts = line.split(":")
            name = parts[0].strip()
            value = parts[1].strip()
            values = value.split(",")
            type = ""
            value = "0"
            for v in values:
                if "type" in v:
                    type = v.split("=")[1].strip().strip(";")
                if "value" in v:
                    value = v.split("=")[1].strip().strip(";")
             
            symbols[name] = { "type": type, "value": value}
            
        elif current_section == "memory":
            parts = line.split(":")
            symbol = parts[0].strip()
            value = parts[1].strip()
            values = value.split(",")
            file = None
            start = None
            size = None
            define = None
            for v in values:
                if "file" in v:
                    file = v.split("=")[1].strip().strip(";")
                if "start" in v:
                    start = v.split("=")[1].strip().strip(";")
                    start = parse_value(start, symbols)

                if "size" in v:
                    size = v.split("=")[1].strip().strip(";")
                    size = parse_value(size, symbols)
                if "define" in v:
                    define = v.split("=")[1].strip().strip(";")
                    if define.lower() == 'yes':
                        define = True
                    else:
                        define = False
            memory[symbol] = {"file": file, "start": start, "size": size, "define": define}

    videoPageSize = 0x1FE0 
    st = symbols['__STACKSIZE__']
    stackSize = parse_value(st['value'], symbols)
    endRam = memory['RAM']['start'] + memory['RAM']['size']
    memAddress = endRam

    memory['STACK'] = {"file": None, "start": memAddress, "size": stackSize , "define": False}
    memAddress += stackSize
    memory['VIDEO1'] = {"file": None, "start": memAddress, "size": videoPageSize,   "define": False}
    memAddress += videoPageSize
    memory['VIDEO2'] = {"file": None, "start": memAddress, "size": videoPageSize  , "define": False}
    memAddress += videoPageSize
    memory['REGS'] = {"file": None, "start": memAddress + stackSize + (videoPageSize * 2), "size": (0xFFFF-0xFFF8), "define": False}
            
    return symbols, memory


def plot_memory_usage(memory_dict, total_size=0xFFFF):
    fig, ax = plt.subplots()
    names = list(memory_dict.keys())
    starts = [x[0] for x in memory_dict.values()]
    sizes = [x[1] for x in memory_dict.values()]
    ax.bar(names, sizes, bottom=starts, align='center', color=['b', 'g', 'r', 'c', 'm'])
    ax.set_ylim(0, total_size)
    for i, v in enumerate(sizes):
        ax.text(i, starts[i] + v/2, f"0x{v:X}", ha='center', color='black', fontweight='bold')
    plt.show()


def parse_segment_list(file, memory):
    segments = {}
    lastSegment = 0
    with open(file, "r") as f:
        lines = f.readlines()

        inSegmentList = False

        for line in lines:
            line = line.strip()
            if line.startswith("Segment list:"):
                inSegmentList = True
                continue
            if inSegmentList:
                if line == "":
                    inSegmentList = False
                    break
                if line.startswith("Name"):
                    continue
                if line.startswith("----"):
                    continue
                if not line:
                    break
                
                name, start, end, size, align = line.split()
                start = int(start, 16)
                end = int(end, 16)
                size = int(size, 16)

                if end > lastSegment:
                    lastSegment = end

                segments[name] = (start, end, size)

    endRam = memory['RAM']['start'] + memory['RAM']['size']
    global g_freeSpace 
    g_freeSpace = endRam  - lastSegment 

    if (g_freeSpace > 0):
        segments["Free Space"] = (lastSegment, endRam,  g_freeSpace)
    else:
        segments["OVER RAM"] = (endRam, lastSegment, g_freeSpace)

    return segments

def plot_segment_list(segment_list, address_space_size, symbols, memory):
    import matplotlib.pyplot as plt

    segments = []
    for name, (start, end, size) in segment_list.items():
        segments.append((name, end - start + 1))

    fig, ax = plt.subplots()
    ax.set_title("Lynx Memory Map")
    ax.set_xlabel("")
    ax.set_ylabel("Address")
    ax.set_ylim([0, address_space_size])
    ax.set_xlim([0, 1])

    current_bottom = 0
    rects1 = []
    rects2 = []
    for name, size in segments:
        rect = ax.bar(0, size, bottom=current_bottom, label=name, width=0.5, align='edge')
        current_bottom += size
        rects1.append(rect)

    # Add another bar
    current_bottom = 0
    for key, val in memory.items():
        size = val['size']
        name = key
        rect = ax.bar(0.5, size, bottom=current_bottom, label=name, width = 0.5, align='edge')
        rects2.append(rect)
        current_bottom += size

    labelCount = 16
    step = address_space_size / labelCount
    labels = []
    for i in range(labelCount+1):
        if i == labelCount:
            labels.append(f"0x{int((i*step)-1):X}")
        else:
            labels.append(f"0x{int(i*step):X}")

    ax2 = ax.twinx()
    ax2.set_yticks([x*step for x in range(len(labels))])
    ax2.set_yticklabels(labels)

    # Add an extra tick and label at the top
    ax.set_yticks([x*step for x in range(len(labels))])
    ax.set_yticklabels(labels )
    ax.yaxis.set_tick_params(pad=15)
    ax.set_yticks([labelCount*step], minor=True)
    ax.set_yticklabels(['FFFF'], minor=True)

    t = ax2.get_ylim()

    threshold = 1024
    addr = 0x0000
    y = 0
    for rect in rects1:
        r = rect[0]
        height = r.get_height()

        if height > threshold:  
            ax.annotate(rect.get_label(),
                        xy=(r.get_x() + r.get_width()/2, y+(height/2)),
                        ha='center', va='center', color='white', size=9)
        addr += height
        y += height

    y = 0 
    index = 0
    memoryItemList = list(memory.items())
    for rect in rects2:
        r = rect[0]
        height = r.get_height()
        name = ""

        key, val = memoryItemList[index]
        index+=1
        name = key

        if height > threshold:  
            ax.annotate(name,
                        xy=(r.get_x() + r.get_width()/2, y+(height/2)),
                        ha='center', va='center', color='white', size=9)
        y += height


    global g_freeSpace
    fig.text(0.5, 0.0, "Free space: " + str(g_freeSpace), ha="center", va="bottom")
    #plt.show()
    plt.savefig("memory_map.png", dpi=400)

symbols, memory = parse_memory_layout("lynx_config.txt")
segments = parse_segment_list("lynx.map", memory)
plot_segment_list(segments, 0x10000, symbols, memory)
