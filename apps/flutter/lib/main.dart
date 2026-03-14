import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:google_fonts/google_fonts.dart';

import 'design_tokens.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  SystemChrome.setEnabledSystemUIMode(SystemUiMode.edgeToEdge);
  SystemChrome.setSystemUIOverlayStyle(
    const SystemUiOverlayStyle(
      systemNavigationBarColor: Colors.transparent,
      systemNavigationBarContrastEnforced: false,
      statusBarColor: Colors.transparent,
      statusBarContrastEnforced: false,
    ),
  );
  runApp(const HumanApp());
}

class HumanApp extends StatelessWidget {
  const HumanApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Human',
      debugShowCheckedModeBanner: false,
      theme: _buildTheme(Brightness.light),
      darkTheme: _buildTheme(Brightness.dark),
      themeMode: ThemeMode.system,
      home: const MainScaffold(),
    );
  }

  ThemeData _buildTheme(Brightness brightness) {
    const seedColor = Color(0xFF7AB648);
    final colorScheme = ColorScheme.fromSeed(
      seedColor: seedColor,
      brightness: brightness,
      primary: seedColor,
    );

    final textTheme = GoogleFonts.nunitoSansTextTheme(
      brightness == Brightness.light
          ? ThemeData.light().textTheme
          : ThemeData.dark().textTheme,
    ).copyWith(
      bodyLarge: textTheme.bodyLarge?.copyWith(
        fontSize: HUTokens.type_role_body_lg_font_size.toDouble(),
        fontWeight: FontWeight.w400,
        height: HUTokens.type_role_body_lg_line_height,
      ),
      bodyMedium: textTheme.bodyMedium?.copyWith(
        fontSize: HUTokens.type_role_body_md_font_size.toDouble(),
        fontWeight: FontWeight.w400,
        height: HUTokens.type_role_body_md_line_height,
      ),
      bodySmall: textTheme.bodySmall?.copyWith(
        fontSize: HUTokens.type_role_body_sm_font_size.toDouble(),
        fontWeight: FontWeight.w400,
        height: HUTokens.type_role_body_sm_line_height,
      ),
      labelLarge: textTheme.labelLarge?.copyWith(
        fontSize: HUTokens.type_role_label_lg_font_size.toDouble(),
        fontWeight: FontWeight.w500,
        height: HUTokens.type_role_label_lg_line_height,
      ),
      labelMedium: textTheme.labelMedium?.copyWith(
        fontSize: HUTokens.type_role_label_md_font_size.toDouble(),
        fontWeight: FontWeight.w500,
        height: HUTokens.type_role_label_md_line_height,
      ),
      labelSmall: textTheme.labelSmall?.copyWith(
        fontSize: HUTokens.type_role_label_sm_font_size.toDouble(),
        fontWeight: FontWeight.w500,
        height: HUTokens.type_role_label_sm_line_height,
      ),
      titleLarge: textTheme.titleLarge?.copyWith(
        fontSize: HUTokens.type_role_title_lg_font_size.toDouble(),
        fontWeight: FontWeight.w500,
        height: HUTokens.type_role_title_lg_line_height,
      ),
      titleMedium: textTheme.titleMedium?.copyWith(
        fontSize: HUTokens.type_role_title_md_font_size.toDouble(),
        fontWeight: FontWeight.w500,
        height: HUTokens.type_role_title_md_line_height,
      ),
      titleSmall: textTheme.titleSmall?.copyWith(
        fontSize: HUTokens.type_role_title_sm_font_size.toDouble(),
        fontWeight: FontWeight.w500,
        height: HUTokens.type_role_title_sm_line_height,
      ),
      headlineMedium: textTheme.headlineMedium?.copyWith(
        fontSize: HUTokens.type_role_headline_md_font_size.toDouble(),
        fontWeight: FontWeight.w600,
        height: HUTokens.type_role_headline_md_line_height,
      ),
      headlineSmall: textTheme.headlineSmall?.copyWith(
        fontSize: HUTokens.type_role_headline_sm_font_size.toDouble(),
        fontWeight: FontWeight.w600,
        height: HUTokens.type_role_headline_sm_line_height,
      ),
    );

    final bg = brightness == Brightness.light
        ? HUTokens.light_bg_surface
        : HUTokens.dark_bg_surface;
    final scaffoldBg = brightness == Brightness.light
        ? HUTokens.light_bg
        : HUTokens.dark_bg;
    final textColor = brightness == Brightness.light
        ? HUTokens.light_text
        : HUTokens.dark_text;

    return ThemeData(
      useMaterial3: true,
      colorScheme: colorScheme.copyWith(
        surface: bg,
        onSurface: textColor,
      ),
      scaffoldBackgroundColor: scaffoldBg,
      textTheme: textTheme.apply(
        bodyColor: textColor,
        displayColor: textColor,
      ),
      appBarTheme: AppBarTheme(
        backgroundColor: Colors.transparent,
        elevation: 0,
        scrolledUnderElevation: 0,
        systemOverlayStyle: SystemUiOverlayStyle(
          statusBarColor: Colors.transparent,
          statusBarIconBrightness:
              brightness == Brightness.light
                  ? Brightness.dark
                  : Brightness.light,
          systemNavigationBarColor: Colors.transparent,
          systemNavigationBarIconBrightness:
              brightness == Brightness.light
                  ? Brightness.dark
                  : Brightness.light,
        ),
      ),
      bottomNavigationBarTheme: BottomNavigationBarThemeData(
        backgroundColor: brightness == Brightness.light
            ? HUTokens.light_bg_surface
            : HUTokens.dark_bg_surface,
        selectedItemColor: seedColor,
        unselectedItemColor: brightness == Brightness.light
            ? HUTokens.light_text_muted
            : HUTokens.dark_text_muted,
      ),
    );
  }
}

class MainScaffold extends StatefulWidget {
  const MainScaffold({super.key});

  @override
  State<MainScaffold> createState() => _MainScaffoldState();
}

class _MainScaffoldState extends State<MainScaffold> {
  int _currentIndex = 0;

  static const _tabs = [
    ('Overview', Icons.dashboard_outlined, Icons.dashboard),
    ('Chat', Icons.chat_bubble_outline, Icons.chat_bubble),
    ('Sessions', Icons.history_outlined, Icons.history),
    ('Settings', Icons.settings_outlined, Icons.settings),
  ];

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: IndexedStack(
        index: _currentIndex,
        children: _tabs
            .map((t) => _PlaceholderTab(name: t.$1))
            .toList(),
      ),
      bottomNavigationBar: Container(
        decoration: BoxDecoration(
          color: Theme.of(context).colorScheme.surface,
        ),
        child: SafeArea(
          child: Padding(
            padding: const EdgeInsets.symmetric(
              horizontal: HUTokens.spacing_md,
              vertical: HUTokens.spacing_sm,
            ),
            child: ClipRRect(
              borderRadius: BorderRadius.circular(HUTokens.radius_lg.toDouble()),
              child: BottomNavigationBar(
                currentIndex: _currentIndex,
                onTap: (i) => setState(() => _currentIndex = i),
                type: BottomNavigationBarType.fixed,
                items: [
                  for (var i = 0; i < _tabs.length; i++)
                    BottomNavigationBarItem(
                      icon: Icon(_currentIndex == i ? _tabs[i].$3 : _tabs[i].$2),
                      label: _tabs[i].$1,
                    ),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }
}

class _PlaceholderTab extends StatelessWidget {
  const _PlaceholderTab({required this.name});

  final String name;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.transparent,
      body: Center(
        child: Text(
          name,
          style: Theme.of(context).textTheme.headlineMedium,
        ),
      ),
    );
  }
}
