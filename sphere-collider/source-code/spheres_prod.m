% SPHERES
% simulation of colllisions and settling of falling spheres
% into a vessel
% (C) 2022-2023 Pavel Strachota

% Version used to obtain the final spheres positions at rest
% This version contains the initial positions of the spheres packed into a denser grid
% so that they don't accelerate so much during free fall (which accelerates the computation :)

global n r h0 R T g dissipation_focusing kin_energy_fraction collision_force_multiplier collision_force_exponent

% 1 = do the simulation, 0 = skip the simulation (a plot from a previous simulation can be performed though)
simulate = 1;
% 0 = no plot, 1 = plot the whole evolution, 2 = only plot the final state
plot_result = 0;


% number of spheres
n = 200;
% sphere radius
r = 0.1;
% initial height of the lowest sphere
h0 = 1.0+r;
% vessel dimensions
R = 1;
% final time
T = 5;
% coefficient of restitution
COR = 0.4;
% focusing of the transition from full force when the collision is in its
% first (approaching) phase and the reduced force when the collision is in
% its second (separation) phase. The higher the value, the thinner is
% the transition (see the rebound() function)
dissipation_focusing = 100;

% parameters of the collision force
collision_force_multiplier = 10;
collision_force_exponent = 15;

% solver
ODE_solver = @ode45;
% sover options
ODE_solver_options = odeset('RelTol',1e-6,'AbsTol',1e-4,'MaxStep',0.01,'Stats','on');

% snapshots
snapshots = 400;

% -----------------------------------------------------------

g = [0 0 -9.81];
%g = [0 0 0];

% kinetic energy fraction maintained after collision
kin_energy_fraction = COR^2;

% initialize initial positions of the spheres in a jittered grid
coords = zeros(n,3);
balls_per_row = floor(R/(2.5*r));
distance = R/balls_per_row;
zi=1; yi=1; xi=1;
for i=1:n
   coords(i,:) = [(xi-0.5)*distance (yi-0.5)*distance h0+(zi-0.5)*distance] + 0.25*r*rand(1,3);
   xi = xi + 1;
   if(xi>balls_per_row)
       xi = 1;
       yi = yi + 1;
       if(yi>balls_per_row)
           yi = 1;
           zi = zi + 1;
       end
   end
end
           
velocities=zeros(n,3);
%velocities(6)=-2;

init_cond = reshape([coords velocities],6*n,1);

% the following line performs the actual simulation; comment it out
% and re-run the script to replay the animation
if(simulate)
    tic;
    disp('Running the simulation ...');
    [t, y] = ODE_solver(@rhs, linspace(0,T,snapshots), init_cond, ODE_solver_options);
    time = toc;
    fprintf('Simulation completed in %g seconds.\n',time);

end

% for a test with a single ball, do this to plot the evolution of the vertical position:
% plot(y(:,3)-r)

if(~plot_result)
    return;
end

disp('Visualizing the results...');

start_with_snapshot = 1;
if(plot_result==2)
        % only plot the final state
        start_with_snapshot = snapshots;
end

[xs,ys,zs] = sphere;

for q=start_with_snapshot:snapshots

    % extract the solution at the i-th snapshot in matrix form
    w = reshape(y(q,:),n,6);

    % plot the current positions of the spheres
    clf
    cmap = colormap('turbo');
    for i=1:n
        xc=r*xs+w(i,1);
        yc=r*ys+w(i,2);
        zc=r*zs+w(i,3);
        surf(xc,yc,zc,'FaceColor',cmap(round(256*i/n),:),'EdgeColor','none')
        hold on
    end
    camlight left
    lighting flat
    axis equal
    axis([-0.1*R 1.1*R -0.1*R 1.1*R 0 5*R]);
    drawnow();
    q
    exportgraphics(gca,['plots/' sprintf('%03d.png',q)]);
end

disp('Done.');

% -----------------

function rebound_factor = rebound(v)
% a smooth version of a function that basically returns
% 1 for v>0
% kin_energy_fraction for v<0
    global kin_energy_fraction dissipation_focusing

    rebound_factor = kin_energy_fraction + 0.5*(1.0-kin_energy_fraction)*(1.0+tanh(v*dissipation_focusing));
end

function cf = collision_factor(surface_distance)
% a collision force factor depending on the distance of the surfaces
% of the colliding objects
    global r collision_force_multiplier collision_force_exponent

    cf = collision_force_multiplier * exp(-(collision_force_exponent*(surface_distance)/r));
end

function dy_dt = rhs(t,y)
    global n g r R
    % reshape to a matrix where each row corresponds to 3 components of
    % position followed by 3 components of velocity of one particle
    y=reshape(y,n,6);
    dy_dt = zeros(n,6);
    % derivative of position is velocity (for all particles at once)
    dy_dt(:,1:3) = y(:,4:6);
    % initialize acceleration of all particles to gravity acceleration
    dy_dt(:,4:6) = ones(n,1)*g;
    for i=1:n
        % repulsive & frictional forces between all particle pairs
        for j=(i+1):n
            % mutual position (j-th w.r.t. i-th particle)
            mp = y(j,1:3) - y(i,1:3);
            distance = norm(mp);
            % mutual velocity
            mv = y(j,4:6) - y(i,4:6);
            % derivative of mutual distance w.r.t. time
            % (shows if the particles are moving toward or away from each other)
            heading = 2 * dot(mp,mv);
            % collisional acceleration: If the particles are approaching each other (heading<0),
            % the forces act as in a perfectly elastic collision. If the
            % particles are moving away from each other, the repulsive
            % forces are multiplied by kin_energy_fraction determined by COR, the coefficient of restitution
            acc = collision_factor(distance-2*r) * rebound(-heading)* mp/distance;
            dy_dt(i,4:6) = dy_dt(i,4:6) - acc;
            dy_dt(j,4:6) = dy_dt(j,4:6) + acc;
        end
        
        % repulsive forces at vessel walls
        acc = [0 0 0];

        % bottom
        distance = y(i,3);
        acc = acc + rebound(-y(i,6)) * collision_factor(distance-r) * [0 0 1];
        % left
        distance = y(i,1);
        acc = acc + rebound(-y(i,4)) * collision_factor(distance-r) * [1 0 0];
        % right
        distance = R-y(i,1);
        acc = acc + rebound(y(i,4)) * collision_factor(distance-r) * [-1 0 0];
        % front
        distance = y(i,2);
        acc = acc + rebound(-y(i,5)) * collision_factor(distance-r) * [0 1 0];
        % rear
        distance = R-y(i,2);
        acc = acc + rebound(y(i,5)) * collision_factor(distance-r) * [0 -1 0];

        dy_dt(i,4:6) = dy_dt(i,4:6) + acc;
    end
    % reshape back to a vector
    dy_dt = reshape(dy_dt, 6*n, 1);
end

