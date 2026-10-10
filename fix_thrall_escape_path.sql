-- Shift Thrall's escape waypoints slightly West to dodge the tree, then back towards the fields
UPDATE script_waypoint SET PositionX=2630.0, PositionY=668.0 WHERE entry=17876 AND Point=125;
UPDATE script_waypoint SET PositionX=2640.0, PositionY=620.0 WHERE entry=17876 AND Point=126;
