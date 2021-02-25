clear all;
addpath('G:\My Drive\espx\data');
format longG
diary summary_cons_SIZE_OF_QUEUE.txt
diary on


%% Consumer %%
consumer_files = dir('cons*.txt');
%consumer_files = consumer_files.name;
i=1;
for file = consumer_files'
    consumer = importdata(file.name);
    file.name
    consumer = summary_stats(consumer)
    i=i+1;
end

diary off
