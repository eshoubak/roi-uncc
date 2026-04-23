UNC Charlotte

GridLAB-D (GLD) Standalone | GridPACK (GPK) Per-Phase Standalone | GLD+GPK HELICS Co-Simulation

Objective
The objective was to verify the consistency of unbalanced GridLAB-D reference translated into full 13-bus per-phase GridPACK model, and confirm the results by coupling the two seperate systems via HELICS by co-simulating the two models by exchanges boundary data across the full 13-bus network

Three configurations (cases) were compared:

1. Gridlab-D standalone — Full 13-bus: gld_13bus.glm. This is found in glm_13bus_standalone folder
2. GridPACK per-phase standalone - Full 13-bus RAW: This is found in gpk_13bus_per_phase_standalone folder
3. Gridlab-D + GridPACK + HELICS co-simulation: This is found in 13_bus_gpk_gld_cosim folder

NOTE:
This model was built and developed by the UNC Charlotte team

Network Description
The Test system is the developed 13-node feeder (3-phase) by the UNC Charlotte team. The nominal voltage (L-N) is 2401.777 V and has 13 nodes in all
The simulation was run for 24 hours

Boundary Bus Setup (Co-Simulation)
The cosim designated bus 3 (Node3) as the boundary bus. At each HELICS timestep:
- GLD publishes the total feeder power Sa/Sb/Sc (measured at Meter1)
- GPK returns the boundary bus voltages Va/Vb/Vc back to GLD

After running each case, the results across the three models on all 13 buses, all 3 phases matched with a difference of < 1×10⁻⁵ pu
