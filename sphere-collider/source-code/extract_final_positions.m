% extracts final positions of the spheres
% The workspace must be in a state after the simulation
% (either completed or loaded from a .mat file)

% final state
w=y(end,:);
% extract positions only
ww=reshape(w,n,6);
pos=ww(:,1:3);
% write to file
writematrix(pos,'spheres_final_positions.txt','Delimiter','tab');
