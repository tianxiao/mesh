some quick comments:

in the example folder there are a three programs compress.exe,
decompress.exe and precisioncheck.exe that should be pretty self
explanatory and the source code is there too. these programs
illustrate how to use the micode compression library.

this library is currently not production quality. it has a few memory
leaks, etc. this is okay. it is only meant for evaluation purposes.

there are also two tools 'sm_viewer' and 'ooc_viewer' that you can
use to visulalize compressed models ... with 'speedcheck' you can
measure how fast the decompressor will operate.

note that the compressed format allows 'streaming decompression'.
the micode library does currently not expose this, but i have a
compressed mesh reader librabry that does this.

              /*********************************************/
              /***            sm_viewer.exe              ***/
              /*********************************************/

The sm_viewer.exe is an OpenGL-based viewer that allows you to look at
the compressed meshes. First of all you can watch the order in which
the triangles stream from disk into memory.

To visualize an uncompressed model call

sm_viewer mesh.ply
sm_viewer mesh.ply.gz   (gzip.exe must be in the path)

To visualize a compressed model simply call

sm_viewer mesh_compressed.ply

The model name *must* contain the substring "_compressed".

The viewer simplifies a mesh on-the-fly using simple vertex clustering.
There is a pop-up  menu for changing step size of the visualization and
grid resolution for the simplification.

Hot Keys:

<p> - play ... either starts loading the original mesh or replays the
      simplified version kept in memory.

<o> - stops (or rather pauses) the replay

<s> - step through the replay. specify the # of steps in the popup-menu 

<c> - change the triangles coloring based on their order

<w> - switch front and back facing materials.

<v> - compute the vertex span for each triangles and color them red in
      respect to its value. use '-' and '=' to decrease / increase the
      contrast in the red rendering
      
<space> - toggle between rotation, translation, zoom 

              /**********************************************/
              /***             ooc_viewer.exe             ***/
              /**********************************************/

The occviewer.exe program comes with a predefined window size of
1200x900. To choose your own window size simply call

ooc_viewer.exe 320 240 mesh_compressed.ply

the ooc_viewer allows you to either look at a visualization of the
decompression process or to render a single frame out-of-core.

there is a pop up menu when you press the right mouse button.

                          -------------------

the following hotkeys are very useful:

'space' : toggle between rotation, translation, zoom

it's a terrible interface, i know. but for this rock-bottom viewer
for which i re-used the code from my first assignment in an OpenGL
course that i took a few years back (-:

'p' : plays the decompression animation
'o' : stops the decompression animation
's' : steps through the decompression animation

'l' : loads a model without animation

and the most fun one:

'r' : render a single frame out-of-core

                          -------------------

the tool decompresses the model and stores a certain number of points
while doing so. it also shows the boundaries that the out-of-core
decompressor is working on.

the point cloud is also useful for choosing a somewhat interesting view
before rendering a frame.

              /*********************************************/
              /***            speedcheck.exe             ***/
              /*********************************************/

The speedcheck.exe program only decompresses the mesh as fast as it can.
If you monitor the size of this process in the Task Manager you will see
that the memory foot-print of this process is very small.

To decompress a model simply call

speedcheckmesh_compressed.ply

The uncompressed data is not stored.
