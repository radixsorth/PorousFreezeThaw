The data in spheres_final_positions.txt from this simulation have been used for the phase field simulations.

This simulation differs from 200-spheres-for-porous-bed-2 (which was performed later) in the following:

1. 'heading' is calculated by (uncommented) line 176. That is, it is obtained as the time derivative
   of the SQUARE of the mutual distance 'distance'. This makes the the computation of rebound_factor
   inconsistent, as it is obtained differently for the collision of two spheres and for the collision
   with the wall

2. The value of 'dissipation_focusing' on line 35 is set to 100 instead of 10.

This results in:

1. much smaller computation time (8 600 s  vs.  37 000 s)
2. the spheres seem a little more elastic - see the videos
