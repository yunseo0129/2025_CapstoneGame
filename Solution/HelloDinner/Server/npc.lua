myid = 99999;

math.randomseed(os.time())

function set_uid(x)
   myid = x;
end

function sleep(n)  -- n초 동안 대기하는 함수
    local t0 = os.clock()
    while os.clock() - t0 <= n do
    end
end

function event_player_move(player)
   player_x = API_get_x(player);
   player_y = API_get_y(player);
   my_x = API_get_x(myid);
   my_y = API_get_y(myid);
   if (player_x == my_x) then
      if (player_y == my_y) then
         API_SendMessage(myid, player, "HELLO");
         for i = 1, 3 do
            randNum = math.random(0, 3)
            API_Move(myid, player, randNum)
            sleep(1) 
         end
         API_SendMessage(myid, player, "BYE")
      end
   end
end
