% calculate the solids volume fraction in the given region
% CSV file import version

from = [0 0 0];
to = [1 1 1];
res = 100;
snap_stride = 2;

% parameters that need to be specfied manually when reading from
% a series of CSV files (with hard-coded filename pattern - see below)
snapshots = 400;
r = 0.1;

eps_s = [];
i=0;
for snap=snap_stride:snap_stride:snapshots
    i = i+1;
    eps_s(i) = epss(r, snap, from, to, res);
    fprintf("Snapshsot %d: eps_s = %f\n", snap, eps_s(i));
end


% ---------------------------------------------------------

function eps_s = epss(r, snapshot, from, to, res)
    % state at the given snapshot
    ww=readmatrix(sprintf('snap_%03d.csv',snapshot));
    % extract positions only
    pos=ww(:,1:3);
    
    sphere_count = size(pos,1);
    hits = 0;
    
    for z=from(3)+((0:res-1)+0.5)*(to(3)-from(3))/res
        for y=from(2)+((0:res-1)+0.5)*(to(2)-from(2))/res
            for x=from(1)+((0:res-1)+0.5)*(to(1)-from(1))/res
                for b=1:sphere_count
                    if(norm(pos(b,:)-[x y z])<=r)
                        hits = hits+1;
                        break;
                    end
                end
            end
        end
    end
    eps_s = hits / (res*res*res);
end
