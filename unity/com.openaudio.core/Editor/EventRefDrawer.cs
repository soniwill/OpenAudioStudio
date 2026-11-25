using System;
using System.IO;
#if UNITY_EDITOR
using UnityEditor;
using UnityEngine;

namespace OpenAudio.Editor
{
    [CustomPropertyDrawer(typeof(EventRefAttribute))]
    public class EventRefDrawer : PropertyDrawer
    {
        private const float ButtonWidth = 70f;
        private const float Spacing = 4f;

        public override void OnGUI(Rect position, SerializedProperty property, GUIContent label)
        {
            if (property.propertyType != SerializedPropertyType.String)
            {
                EditorGUI.LabelField(position, label.text, "Use [EventRef] only with string fields.");
                return;
            }

            EditorGUI.BeginProperty(position, label, property);

            Rect fieldRect = new Rect(position.x, position.y, position.width - ButtonWidth - Spacing, position.height);
            Rect buttonRect = new Rect(position.x + position.width - ButtonWidth, position.y, ButtonWidth, position.height);

            // Draw normal text field
            property.stringValue = EditorGUI.TextField(fieldRect, label, property.stringValue);

            // Draw browse button
            if (GUI.Button(buttonRect, "Browse...", EditorStyles.miniButton))
            {
                ShowEventPickerMenu(property);
            }

            EditorGUI.EndProperty();
        }

        private void ShowEventPickerMenu(SerializedProperty property)
        {
            var menu = new GenericMenu();
            menu.AddItem(new GUIContent("<None>"), string.IsNullOrEmpty(property.stringValue), () =>
            {
                property.stringValue = string.Empty;
                property.serializedObject.ApplyModifiedProperties();
            });

            string searchPath = Application.streamingAssetsPath;
            var paths = CatalogCache.GetAllKnownEventPaths(searchPath);

            if (paths.Count == 0)
            {
                // Try searching the entire Assets folder if StreamingAssets has no catalogs
                paths = CatalogCache.GetAllKnownEventPaths(Application.dataPath);
            }

            if (paths.Count == 0)
            {
                menu.AddDisabledItem(new GUIContent("No .bank.catalog.json files found"));
            }
            else
            {
                menu.AddSeparator(string.Empty);
                foreach (var path in paths)
                {
                    string menuPath = path.Replace(":", "/");
                    if (menuPath.StartsWith("/")) menuPath = menuPath.Substring(1);

                    bool isSelected = string.Equals(property.stringValue, path, StringComparison.Ordinal);
                    menu.AddItem(new GUIContent(menuPath), isSelected, (userData) =>
                    {
                        property.stringValue = (string)userData;
                        property.serializedObject.ApplyModifiedProperties();
                    }, path);
                }
            }

            menu.ShowAsContext();
        }
    }
}
#endif
