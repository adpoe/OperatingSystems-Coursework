""" Model of page tables for a 32-bit address space
"""


class Tree():
    """ Tree for holding our page table entries
    """
    def __init__(self):
        return



class MultiLevelPageTable():
    def __init__(self, how_many_frames):
        # addresses
        self.virtualAddress = None
        self.physicalAddress = None

        # mappings
        self.outerTable = None
        self.pageTableOffset = None 
        self.frameOffset = None       # 4kb or 2^12 indices

        # frames
        self.num_frames = how_many_frames

        # components
        self.OuterTable = OuterPageTable()
        self.SecondLevelTable = SecondLevelPageTable()
        return

class OuterPageTable():
    def __init__(self):
        return

class SecondLevelPageTable():
    def __init__(self):
        return

class Page():
    def __init__(self):
        return

class Frame():
    def __init__(self):
        self.dirty = False
        self.in_use = False
        self.PPN = 0
        return

class FrameTable():
    def __init__(self, how_many_frames):
        self.num_frames = how_many_frames

        self.frame_table = []

        for frame in range (0, self.num_frames):
            next_frame = Frame()
            next_frame.dirty = False
            next_frame.in_use = False
            # anything else we need to add, can add here
            self.frame_table.append(next_frame)
        return

class Disk():
    def __init__(self):
        return

class Cache():
    def __init__(self):
        return
