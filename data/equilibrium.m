close all;

data_sim = dlmread('/usr/local/share/krang-sim-ach/out');
data_bal = dlmread('/usr/local/share/krang/balancing/out');

km = 12.0/141.61; % Nm/A
GR = 15.0;

time = data_bal(:,1);
th_com = data_bal(:,2);
dth_com = data_bal(:,3);
th_wheel = data_bal(:,4);
dth_wheel = data_bal(:,5);
th_spin = data_bal(:,6);
dth_spin = data_bal(:,7);
tau_l = data_bal(:,8)*km*GR;
tau_r = data_bal(:,9)*km*GR;
K_th_com = data_bal(:,10);
K_dth_com = data_bal(:,11);
K_th_wheel = data_bal(:,12);
K_dth_wheel = data_bal(:,13);
K_th_spin = data_bal(:,14);
K_dth_spin = data_bal(:,15);
mass = data_bal(:,16);
com_est_x = data_bal(:,17);
com_est_y = data_bal(:,18);
com_est_z = data_bal(:,19);
alpha_eso = data_bal(:,44);
beta_eso = data_bal(:,45);

com_x = data_sim(:, 2);
com_y = data_sim(:, 3);
com_z = data_sim(:, 4);

g = 9.8;
l = (com_est_x.^2 + com_est_z.^2).^0.5;
tau_disturb = mass.*g.*(com_x-com_est_x);
th_com_true = atan2(com_x, com_z);

figure;
subplot(3,1,1);
plot(time, mass.*g.*l.*sin(th_com), time, -tau_disturb + tau_l + tau_r, time, tau_l + tau_r); 
legend({'$$mgl \sin(\theta_{com})$$', '$$-\tau_{disturb} + \tau$$', '$$\tau$$'}, 'Interpreter', 'latex');
grid on;

subplot(3,1,2);
plot(time, tau_l+tau_r, ...
    time, K_th_com.*th_com*km*GR, ...
    time, K_dth_com.*dth_com*km*GR, ...
    time, K_th_wheel.*th_wheel*km*GR, ...
    time, K_dth_wheel.*dth_wheel*km*GR, ...
    time, K_th_spin.*th_spin*km*GR, ...
    time, K_dth_spin.*dth_spin*km*GR, 'LineWidth', 3); 
legend({'$$\tau$$', '$$K_{\theta com} \theta_{com}$$', ...
    '$$K_{\dot \theta com} \dot \theta_{com}$$', ...
    '$$K_{\theta wheel} \theta_{wheel}$$', ...
    '$$K_{\dot \theta wheel} \dot \theta_{wheel}$$', ...
    '$$K_{\theta spin} \theta_{spin}$$', ...
    '$$K_{\dot \theta spin} \dot \theta_{spin}$$'}, 'Interpreter', 'latex');
grid on;

subplot(3,1,3);
plot(time, th_wheel, time, -K_th_com./K_th_wheel.*th_com, time, th_com_true*180.0/pi, time, alpha_eso, time, beta_eso); 
legend({'$$\theta_{wheel}$$', '$$-K_{\theta com} \theta_{com} / K_{\theta wh}$$', '$$\theta_{com}^{true}$$', '$$\alpha_{eso}$$', '$$\beta_{eso}$$'}, ...
    'Interpreter', 'latex');
grid on;

% figure;
% plot(time, com_x, time, com_y, time, com_z, time, com_est_x, ...
%     time, com_est_y, time, com_est_z);
% legend({'$$x_{com}$$', '$$y_{com}$$', '$$z_{com}$$', ...
%     '$$x_{com}^{est}$$', '$$y_{com}^{est}$$', '$$z_{com}^{est}$$'}, ...
%     'Interpreter', 'latex');
% grid on;

% figure;
% positions_bal = data_bal(:,end-23:end);
% positions_sim = data_sim(:,end-23:end);
% % pos_diff = positions_bal - positions_sim;
% % for i=1:4
% %     subplot(2,2,i);
% %     plot(time, pos_diff(:,(i-1)*2+(1:2))); 
% %     legend();
% % end
% for i=1:3
%     subplot(3,1,i)
%     plot(time, positions_bal(:,i), time, positions_sim(:,i));
%     legend();
% end




