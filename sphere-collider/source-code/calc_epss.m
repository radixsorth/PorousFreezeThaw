% calculate the solids volume fraction in the given region

from = [0 0 0];
to = [1 1 1];
res = 100;
snap_stride = 2;

eps_s = [];
i=0;
for snap=snap_stride:snap_stride:snapshots
    i = i+1;
    eps_s(i) = epss(r, y, snap, from, to, res);
    fprintf("Snapshsot %d: eps_s = %f\n", snap, eps_s(i));
end


% ---------------------------------------------------------

function eps_s = epss(r, y, snapshot, from, to, res)
    % state at the given snapshot
    w=y(snapshot,:);
    % extract positions only
    ww=reshape(w,size(w,2)/6,6);
    pos=ww(:,1:3);
    
    sphere_count = size(pos,1);
    hits = 0;
    
    for z=linspace(from(3),to(3),res)
        for y=linspace(from(2),to(2),res)
            for x=linspace(from(1),to(1),res)
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
