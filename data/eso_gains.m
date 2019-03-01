% Observer Gains
C_ = [1 0 0];
A_ = [[0;0], eye(2); 0,0,0]; 

% A matrix for observers
% we want max overshoot = 10% and 2% settling time of .75 s
percent_OS = .05;
eps = sqrt(log(percent_OS)^2/(pi^2+log(percent_OS)^2));
t_2p = 0.25;
w_n = 4/(t_2p*eps);
r1 = roots([1, 2*eps*w_n, w_n^2]);
L_1 = place(A_',C_',[-100*2*pi,r1'])';
L_1'
