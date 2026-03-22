package com.winlator.cmod.bigpicture.steamgrid;

import com.google.gson.annotations.SerializedName;

import java.util.List;

public class SteamGridGridsResponse {

    @SerializedName("success")
    public boolean success;

    @SerializedName("page")
    public int page;

    @SerializedName("total")
    public int total;

    @SerializedName("limit")
    public int limit;

    @SerializedName("data")
    public List<Grid> data;  // The data field is a list of Grid objects

    public class Grid {
        @SerializedName("id")
        public int id;

        @SerializedName("score")
        public int score;

        @SerializedName("style")
        public String style;

        @SerializedName("url")
        public String url;

        @SerializedName("thumb")
        public String thumb;

        @SerializedName("tags")
        public List<String> tags;

        @SerializedName("author")
        public Author author;
    }

    public class Author {
        @SerializedName("name")
        public String name;

        @SerializedName("steam64")
        public String steam64;

        @SerializedName("avatar")
        public String avatar;
    }
}
