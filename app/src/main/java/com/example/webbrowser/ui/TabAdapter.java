package com.example.webbrowser.ui;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.core.content.ContextCompat;
import androidx.recyclerview.widget.RecyclerView;

import com.example.webbrowser.R;
import com.example.webbrowser.Tab;
import com.google.android.material.chip.Chip;

import java.util.List;

public class TabAdapter extends RecyclerView.Adapter<TabAdapter.TabViewHolder> {
    public interface TabListener {
        void onTabSelected(Tab tab);

        void onTabClosed(Tab tab);
    }

    private final List<Tab> tabs;
    private final TabListener listener;
    private String selectedTabId;

    public TabAdapter(List<Tab> tabs, TabListener listener) {
        this.tabs = tabs;
        this.listener = listener;
    }

    public void setSelectedTabId(String selectedTabId) {
        this.selectedTabId = selectedTabId;
        notifyDataSetChanged();
    }

    @NonNull
    @Override
    public TabViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext()).inflate(R.layout.item_tab, parent, false);
        return new TabViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull TabViewHolder holder, int position) {
        Tab tab = tabs.get(position);
        holder.bind(tab, tab.getId().equals(selectedTabId));
    }

    @Override
    public int getItemCount() {
        return tabs.size();
    }

    class TabViewHolder extends RecyclerView.ViewHolder {
        private final Chip chip;

        TabViewHolder(@NonNull View itemView) {
            super(itemView);
            chip = (Chip) itemView;
        }

        void bind(Tab tab, boolean selected) {
            chip.setText(tab.getTitle());
            chip.setChipBackgroundColorResource(selected ? R.color.secondaryVariant : R.color.primaryVariant);
            int color = ContextCompat.getColor(itemView.getContext(), selected ? R.color.onSecondary : R.color.onSurface);
            chip.setTextColor(color);
            chip.setOnClickListener(v -> listener.onTabSelected(tab));
            chip.setOnCloseIconClickListener(v -> listener.onTabClosed(tab));
        }
    }
}
