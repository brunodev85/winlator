package com.winlator.cmod.bigpicture.steamgrid;

import com.google.gson.annotations.SerializedName;

import java.util.List;

public class SteamGridSearchResponse {
    @SerializedName("success")
    public boolean success;

    @SerializedName("data")
    public List<GameData> data;

    public class GameData {
        @SerializedName("id")
        public int id;

        @SerializedName("name")
        public String name;

        @SerializedName("url")
        public String url;
    }
}
