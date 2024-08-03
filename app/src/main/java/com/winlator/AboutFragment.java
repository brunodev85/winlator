package com.winlator;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.text.Html;
import android.text.method.LinkMovementMethod;
import android.widget.LinearLayout;
import android.widget.TextView;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;


public class AboutFragment extends Fragment {
    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.fragment_about, container, false);
        ((AppCompatActivity)getActivity()).getSupportActionBar().setTitle("About");

        String[] credits = {
                "Ubuntu RootFs (<a href=\"https://releases.ubuntu.com/focal\">Focal Fossa</a>)",
                "Wine (<a href=\"https://www.winehq.org\">winehq.org</a>)",
                "Box86/Box64 (<a href=\"https://github.com/ptitSeb\">ptitseb</a>)",
                "PRoot (<a href=\"https://proot-me.github.io\">proot-me.github.io</a>)",
                "Mesa (Turnip/Zink/VirGL) (<a href=\"https://www.mesa3d.org\">mesa3d.org</a>)",
                "DXVK (<a href=\"https://github.com/doitsujin/dxvk\">github.com/doitsujin/dxvk</a>)",
                "VKD3D (<a href=\"https://gitlab.winehq.org/wine/vkd3d\">gitlab.winehq.org/wine/vkd3d</a>)",
                "D8VK (<a href=\"https://github.com/AlpyneDreams/d8vk\">github.com/AlpyneDreams/d8vk</a>)",
                "CNC DDraw (<a href=\"https://github.com/FunkyFr3sh/cnc-ddraw\">github.com/FunkyFr3sh/cnc-ddraw</a>)"
        };

        LinearLayout creditsContainer = view.findViewById(R.id.credits);

        for (String credit : credits) {
            TextView tvCredit = new TextView(getContext());
            tvCredit.setText(Html.fromHtml(credit, Html.FROM_HTML_MODE_LEGACY));
            tvCredit.setMovementMethod(LinkMovementMethod.getInstance());
            creditsContainer.addView(tvCredit);
        }

        view.findViewById(R.id.button_website).setOnClickListener(v -> openWebURL("https://www.winlator.org"));
        view.findViewById(R.id.button_github).setOnClickListener(v -> openWebURL("https://github.com/brunodev85/winlator"));

        return view;
    }
    public void openWebURL( String inURL ) {
        Intent browse = new Intent( Intent.ACTION_VIEW , Uri.parse( inURL ) );
        startActivity( browse );
    }
}