from __future__ import print_function

import os
import sys

from sam import *


def main():
    if len(sys.argv) != 3:
        raise RuntimeError("usage: sam_export_integration.py input.h5 output_base")

    input_path = os.path.abspath(sys.argv[1])
    output_base = os.path.abspath(sys.argv[2])
    output_dir = os.path.dirname(output_base)
    if output_dir and not os.path.isdir(output_dir):
        os.makedirs(output_dir)

    import VTUFileIO
    odb = session.importHDF5FileNewOdb(name=input_path)
    session.viewports['Viewport: 1'].setValues(displayedObject=odb)
    mdb.models['Model-1'].exportOdbToVtk(output_base, input_path, 0, 'Model-1')
    print("VTK integration export completed:", output_base)


if __name__ == '__main__':
    main()
